# LPProcessor

A Narrative Parser

# Contents

[Research Directions 3](#_Toc46105737)

[Overview 3](#_Toc46105738)

[Parsing Processing Stages 4](#_Toc46105739)

[Question Answering 17](#_Toc46105740)

[Processing Stages 17](#_Toc46105741)

[Other important routines 18](#_Toc46105742)

[Input sources 18](#_Toc46105743)

[Testing 20](#_Toc46105744)

[Linguistics Studies 20](#_Toc46105745)

[Increasing Accuracy of POS detection 20](#_Toc46105746)

[The Role of a Relativizer 21](#_Toc46105747)

[CoreNLP Comparison: 22](#_Toc46105748)

[Third Party Software 24](#_Toc46105749)

[Customized Paice/Husk stemmer 24](#_Toc46105750)

[BING Search 24](#_Toc46105751)

[WordNet 2.1 24](#_Toc46105752)

[MySQL DB 24](#_Toc46105753)

[Updating Gutenberg 26](#_Toc46105754)

[Open Library Internet Archive 26](#_Toc46105755)

[Loading DBPedia into Virtuoso 27](#_Toc46105756)

[MySQL DB Tables 30](#_Toc46105757)

[Solr Performance Comparison 33](#_Toc46105758)

[Appendix 34](#_Toc46105759)

[Command Line 34](#_Toc46105760)

[Object Roles 34](#_Toc46105761)

[Object Classes 36](#_Toc46105762)

[Object Subtypes 37](#_Toc46105763)

[Verb Senses 37](#_Toc46105764)

[Extended Verb Classes 38](#_Toc46105765)

[Ontology 40](#_Toc46105766)

[Syntactic Relations 41](#_Toc46105767)

[Disk Space Usage: 42](#_Toc46105768)

[References 42](#_Toc46105769)

## Research Directions

1. Compare parsing output both in breadth, in depth and in accuracy to the Stanford CoreNLP:

[https://nlp.stanford.edu/projects/stat-parsing.shtml](https://nlp.stanford.edu/projects/stat-parsing.shtml)

https://people.eecs.berkeley.edu/~klein/cs294-5/chen\_goodman.pdf

[https://west.uni-koblenz.de/sites/default/files/BachelorArbeit\_MartinKoerner.pdf](https://west.uni-koblenz.de/sites/default/files/BachelorArbeit_MartinKoerner.pdf)

[https://kheafield.com/code/kenlm/](https://kheafield.com/code/kenlm/)

implementations:

[https://github.com/smilli/kneser-ney](https://github.com/smilli/kneser-ney) (simple)

[https://github.com/kpu/kenlm](https://github.com/kpu/kenlm) (complex but optimized)

1. Go through TREC question answering

TODO (long range):

1. Incorporate automatic clustering (Spark) to replace static ontology
2. Create a 3D environment to furnish physical world understanding

Detailed timeline of development contained in Development Diary in OneDrive documents

## Overview

**Project** : Computational Linguistics Language Processor

**Purpose** : Make computers friendlier to people by predicting how people react in everyday situations, by understanding the human experience of time, and how people talk and refer to each other. This information is collected by analyzing conversations, characters and expressions of time and space for over 5500 hand-picked novels of the Gutenberg online library. Question answering is being developed as an API to the information collected.

**Design Philosophy** : Maximize the analysis of text (as opposed to commercial and most research efforts which seek to minimize the analysis of text), which allows even large novels to be processed as a whole.

**Major Processing Features:**

1. Parses with both syntactic roles and semantic statistical analysis using over 500 patterns, which can accept unresolved ambiguity (sentences with more than one parse, which are common). Semantic statistics were originally collected on British National Corpus.
2. Resolves all pronouns of 1st, 2nd and 3rd person (previous academic work only addresses certain pronouns and only in the 3rd person, as 1st and 2nd person pronouns requires an understanding of audience and narrator). In addition there are other classes of pronouns such as 'other', 'both', 'any' which are also processed, Nationalities ('Irish'), Ordered Pronouns ('first', 'second', 'third'), Gendered Nouns (man, grandfather, etc), and many others.
3. Understands conversations and resolves speakers using many rules such as

**Hail:**

"Look, Bob, this is going to take a while."

**Backwards speaker resolving:**

"Go to the ladder"

"What?"

"I said descend the ladder now!", Martha screamed.

**Spatial relationships:**

She left.

"I just don't understand him."

**Grouping:**

Bob and Bill went toward the train while I held the track lever.

"Pull it towards you!" they shouted.

**Meta Resolve:**

"The gorilla needs to survive", the other man murmured softly.

**Letters, stories (a character telling another character a story relating two or more other characters) etc.**

1. This simple requirement of understanding conversations forces a much higher standard of analysis as we must keep track of people throughout a document, tracking them through space and time as we must resolve not only who is speaking, but who is being spoken to. If someone leaves, we must understand whether the next paragraph is being spoken by the person who has just left, or the people who have been left behind. This implies a sophisticated understanding of grouping people together because rarely are there only two people in a scene. This includes people spying on other groups of people, and determining point of view (who is narrating a particular passage).
2. The ontology is based on YAGO and other ontologies which have been blended together.
3. Database sources include Music Brainz, Wikipedia, DBpedia (for RDF types), Freebase, VerbNet, WordNet, Merriam-Webster Dictionary, Cambridge International Dictionary, thesaurus.com, Roget's Thesaurus, and the U.S. Census (for names and nicknames). Online sources such as DBpedia and Freebase have been localized to minimize internet traffic. I am currently experimenting with Twitter feeds, NewsBank, movie scripts, sentiment analysis using Amazon, etc.
4. Based on many academic sources such as Paice/Husk Stemmer, Lappin and Leass, Martha Palmer, Christiane Fellbaum and team, IBM/Lancaster, Inderjeet Mani, Winograd, Longman Grammar, TimeML, Chomsky, and many others.
5. Question Answering is based on TREC data and papers. Google and BING searches are used instead of the data from NIST. Question answers are based off of syntax but also include creation of a semantic web to infer relations that are not syntactically specified. This includes extraction of data in HTML tables and subquery analysis ("What prize which originated in Spain has Krugman won?")
6. SQL (MySQL) backend, C/C++ middle layer, Java front end. Parallel processing is used for parsing and semantic analysis.

## Parsing Processing Stages

1. **Initialization**
  1. MYSQL Database initialization
  2. Word initialization
    1. Read forms from database
    2. Read Levin verb classes and names
    3. Read VBNet
    4. Initialize time categories
    5. Read holiday lists
    6. Read words having spaces including all places (cities, towns, other geographic data etc)
  3. Read generic word definitions from database
  4. Read noun verb mapping derived from wordNet used in question processing
  5. Initialize sentence patterns
2. **Reading Source (readSourceBuffer)**
  1. Determine encoding (unicode, UTF8, 1252, 8859, ASCII)
  2. Find beginning of actual text (rather than the gutenberg prefix lawyer boilerplate, prefaces, table of contents, etc)
  3. Update start and encoding and steps taken in database
  4. Read text from file
3. **Tokenization (tokenize)**
  1. Read word by word
  2. Words are derived originally from the collegiate Collegiate Merriam Webster dictionary (source file getDictionary.cpp) – this gets possible word classes (using the API). Contains these sections of code for deriving words:
    1. HTML parsing code (reduction of HTML into lexical and definition entries)
    2. Derive completely inflected words from Webster inflection list (ex. -ed)
    3. Derive inflected words from word tense list (ex. past part. , past part & nonstandard past of)
    4. Derive inflected words from (ex. comparative of)
    5. Get main entry (ex. Edit would be the main entry of Edited)
    6. Verify added inflections are legal
    7. Trace variants of words (as listed in Webster)
    8. Trace alternates (adjective or noun, adjective & adverb)
    9. Trace functions and instructions
    10. Trace option processing (other tightly related words, or references into other dictionaries, like the medical dictionary)
    11. Getting the root of any word through prefix and suffix processing (customized Paice/Husk stemmer)
    12. detecting when a word is two words combined together (splitWord)
    13. test routines for verifying word classes/inflections/derivations/mainEntry
  3. Detect meta language ~~ to toggle logging of features
  4. Determine end of sentence, end of paragraph, end of source (avoid indexes, extended Gutenberg boilerplate)
  5. Detect special cases:
    1. plural numbers
    2. rdinal numbers
    3. plain numbers
    4. dates
    5. times
    6. telephone numbers
    7. money
    8. web addresses
    9. dashed words (multiple embedded dashes, different kinds of dashes, etc)
  6. Perform preliminary proper noun analysis through accumulation of statistics and previous information from DB.
  7. Embed flags to note spaces before and after tokens to help with later quote analysis
  8. For each sentence: (doQuotesOwnershipAndContractions)
    1. Do more proper name processing
    2. Determine whether quotes are primary of secondary, and whether they determine someone speaking, or whether they are merely quoted strings – handle single quotes, double quotes and other quote characters from unicode (which may also dictate beginning and end double or single quotes)
    3. Determine noun ownership through further quote processing
    4. Adjust words and explicitly disambiguate ownership from ishasdoes ('what's' is not ownership, it is 'what is', also convert common slang into standard English (wanna -\> want to) etc.
    5. determine when a sentence begins/ends (also populates sentenceStarts array)
    6. determine if a word or phrase is a proper noun or a name – this tries to differentiate between titles, capitalized words at the beginning of sentences. Also uses probability based on how the same word is used in the same text, whether the word is completely capitalized, words surrounding it like honorifics, abbreviations and articles.
    7. Adds or deletes quotations. Changes singular and plural noun owners. Attempts to disambiguate single quotes that are possessive but have no s afterwards.
    8. Orders double and single quotes. Marks embedded quotes, with beginning and end quotations.
4. **Parsing**

Patterns model:

Defined in code, not in an external file, though there are prototype functions for exporting to XML. Each pattern looks like this:

cPattern::create(L"PATTERN\_NAME{PATTERN\_TAGS}",

NUM\_FORM/PATTERNS, L"formclass{TAG}", InflectionFlags, MIN\_OCCURRENCES, MAX\_OCCURRENCES,

…

0);

Each pattern name is preceded by one or two underlines, in all caps. The more underlines, the less frequent or the more it is expected to be a subpattern (\_\_NOUN). This is optionally followed by a {}, which contains pattern flags, separated by colons (:):

| \_PATTERN TAG NAME | Description |
| --- | --- |
| \_BLOCK | Blocks pattern tag collection |
| \_FINAL\_IF\_ALONE | Pattern will not be marked for deletion if lowest cost and surrounded by separators (a form which is marked as top level in DB table forms) |
| \_FINAL | Pattern will not be marked for deletion if lowest cost |
| \_FINAL\_IF\_NO\_MIDDLE\_MATCH\_EXCEPT\_SUBPATTERN | Pattern will not be marked for deletion if it is alone OR if a subpattern. |
| \_ONLY\_BEGIN\_MATCH | Pattern will only be matched if there is a separator or a relativizer at the beginning. |
| \_AFTER\_QUOTE | The pattern must be after a quote to be matched. |
| \_STRICT\_NO\_MIDDLE\_MATCH | The pattern must be after a nonalphanumeric word to be matched, and come immediately before a nonalphanumeric word, unless the last word in the pattern is an abbreviation form. |
| \_ONLY\_END\_MATCH | The pattern must come immediately before a nonalphanumeric word, unless the last word in the pattern is an abbreviation form. |
| \_NO\_REPEAT | The pattern cannot immediately follow itself. |
| \_IGNORE | Don't use for the main parsing matching |
| \_QUESTION | Denotes a question being asked (used to create question/response pairs). Will also cost more if not ending in a question mark, and change agreement behavior if passive. |
| \_NOT\_AFTER\_PRONOUN | Cannot match after a pronoun which could be a subject (I, we, etc) |
| \_EXPLICIT\_SUBJECT\_VERB\_AGREEMENT | Change cost according to subject/verb agreement internal to the pattern. |
| \_EXPLICIT\_NOUN\_DETERMINER\_AGREEMENT | Change cost of noun determiner agreement internal to the pattern. |

If a pattern is not a top level match, but eliminating the pattern will cause the words to be completely unmatched, the pattern will be kept. However, if a pattern that is not marked final survives and is lowest cost, thus eliminating a final pattern, and then is eliminated because it is not final, the phrase will still be unmatched.

'NUM\_FORM/PATTERNS' is the number of forms or patterns following that will be matched as an 'OR' at this step in the pattern. If an explicit word is included in the line, then if that word is matched, then there will be no more matches tried. Otherwise, every form or pattern on that step will be tried. Each form/pattern listed on the step has the following string elements:

form/pattern|word[\*]\*C{TAG:TAG}

form/patternis the form expected. This may also be the name of a pattern. This is required.

word is an explicit word. Only this word, in the specified form, will be matched. This is optional.

[\*] will include all patterns of this name. There are multiple \_NOUN patterns for example. This is only mandatory if all instances of this pattern are not found before this pattern. This is actually to help with pattern formation because if an instance of this pattern is created elsewhere, it may not be a best fit for other patterns that use it so this must be evaluated. This requirement is a reminder.

\*C is the cost of the form or pattern. This is optional and may be negative.

{TAG:TAG} these are tags associated with each part of the step of the pattern. These are optional.

Tags

| TAG NAME | Description |
| --- | --- |
| ADJ | Denotes the position of all adjectives |
| ADJOBJECT | Used if an adjective is used as a subject. 'The former' |
| ADV | Denotes the position of all adverbs |
| ADVOBJECT | Used in computation of verb after verb usage (cost) |
| ANY | Used in a name: a single name not necessarily first or last |
| BUS | Used in a name: A business name |
| DATESPEC | A.D./B.C. |
| DAYMONTH | In a date pattern, the location of the day of the month. |
| DAYWEEK | In a date pattern, the location of the day of the week. |
| DET | The location of the determiner in a noun phrase. |
| EVAL | A tag denoting that a pattern should be evaluated for agreement. |
| FIRST | Used in a name: a first name |
| GNOUN | Used in a name: a noun which is amorphous, it cannot be broken down further into meaningful syntax (address, date, time, telephone number, number) |
| HAIL | A position designating the person being spoken to. |
| HOBJECT | An object of a BARE\_INF pattern – I will make you do this. 'will make' - verbverb, 'you' – HOBJECT 'do' – V\_OBJECT 'this' - OBJECT |
| HOLIDAY | Designating the position of a holiday noun |
| HON | Honorific – Mr. |
| HON2 | Honorific – Mr. President |
| HON3 | Honorific – three honorifics in a row. |
| HOUR | The position in a time pattern designating the hour of time |
| IOBJECT | The immediate object of an infinitive phrase |
| ITO | The position of the 'to' of the infinitive phrase |
| IVERB | Designating the entire infinitive phrase |
| LAST | Used in a name: the position of the last name |
| MIDDLE | Used in a name: the position of the middle name |
| MINUTE | In a time pattern the position of the minutes |
| MNOUN | A noun represented syntactically by a single entity but which is made up of multiple nouns (blue, gray and purple) |
| MOBJECT | Designating a pattern of multiple objects |
| MONTH | The position of a month in any pattern |
| MVERB | A complex verb structure |
| NAME | Any name pattern |
| NAMEOWNER | A name that has a 's or ' at the end |
| NAME\_ABOUT | A word which is linked to a name. |
| NAME\_PRIMARY | A name which is in an expression which adds more information linking it to NAME\_SECONDARY |
| NAME\_SECONDARY | A name which is linked to a NAME\_PRIMARY |
| NOUN | The entire noun pattern, evaluated for noun determiner agreement |
| N\_AGREE | The specific part of the noun pattern which is used to agree with a determiner |
| OBJECT | Used for verb/object agreement |
| P | The preposition of a prepositional phrase |
| PLURAL | A plural structure (two or more nouns) |
| PNOUN | A structure restricted to pronomial nouns |
| PREP | A structure which contains pieces of a prepositional phrase that has been broken up or should have special treatment (At which university does Krugman teach?) |
| PREPOBJECT | The object of a preposition |
| PT | Particle |
| QLAST | letter followed by a quote that is a last name |
| QTYPE | The relativizer starting a question (Who/What/When/etc) |
| REL | A relative phrase that can be attached to an object |
| RE\_OBJECT | The location of a restatement of another noun phrase |
| SEASON | Denoting a season (winter/fall/etc) |
| SINGULAR | A singular noun structure |
| SUBJECT | A subject of a sentence |
| SUBJUNCTIVE | The sentence belongs to a larger pattern suggesting a subjunctive mood |
| SUBOBJECT | An object in a pattern which has been tagged OBJECT. |
| SUFFIX | A name suffix |
| S\_IN\_REL | A sentence in a relative clause |
| TIMECAPACITY | Anywhere a time unit or a day unit appears in a pattern |
| TIMEMODIFIER | The determiner before any time word form |
| TIMESPEC | A.M. or P.M. |
| TIMETYPE | The last word in a time expression like ( 9 past or 10 to) |
| VERB | A tag encompassing the entire verb with conditionals, adverbs and other associated word forms. |
| VERB2 | The verb which is occurring in an \_MSTAIL pattern (a verb which would be considered the second verb because the primary verb is in the \_\_S1 pattern) |
| VNOUN | A verb which is acting as a noun (subject or object of verb) |
| V\_AGREE | The part of the verb which is used for subject/verb agreement tests |
| V\_HOBJECT | The part of the verb that is used for verb object agreement tests. The object used for the verb object test is tagged with an HOBJECT. This is part of a BARE\_INF pattern. |
| V\_OBJECT | The part of the verb that is used for verb object agreement tests. The object used for the verb object test is tagged with an OBJECT. |
| YEAR | The part of a \_DATE pattern designating the year. |
| but | The noun after 'but' |
| conditional | Marks conditional parts of the verb, used for agreement, verb tense |
| future | Used for verb tense |
| id | Flags an identity relationship (be) |
| imp | Flags an imperative tense (do) |
| never | Flags negation for semantic relations |
| no | Flags negation for semantic relations |
| not | Flags negation for semantic relations |
| past | Designates past tense |
| vAB | Look at Verb Senses section |
| vABC |
| vABCD |
| vABD |
| vAC |
| vACD |
| vAD |
| vAD |
| vB |
| vBC |
| vBCD |
| vBD |
| vC |
| vCD |
| vD |
| vE |
| vS |
| vrB |
| vrBC |
| vrBD |
| vrD |

  1. For each sentence:
    1. Set logging
    2. Match patterns against sentence (matchPatternsAgainstSentence)
    3. Print sentence in log depending on debug settings
    4. refreshWordRelations – get the probability of relations between words
    5. Add costs depending on whether patterns should be alone (like Sentence patterns) or not (single nouns or verbs) (addCostFromRecalculatingAloneness)
    6. Mark patterns by cost through further analysis and determine lowest cost patterns (eliminateLoserPatterns). For each token:
      1. Determine if the pattern is designated a top level pattern (a sentence), taking into account context such as sentence boundaries
      2. PHASE 1: Preliminarily eliminate patterns from being winners by comparing their average cost over their length to the lowest average cost of all patterns matched against that position, with some leeway to include patterns that may be lowered in cost due to further processing and compensation due to pattern length (preferring longer patterns)
      3. Assess cost
        1. noun determiner
          1. verb before/verb after assessment
          2. before 'to' with no determiner heavy cost (probably verb)
          3. from X to Y excepting to (2) noun no cost and skip determiner cost
        2. subject, verb agreement
          1. lowest subject cost disambiguation
          2. subject/verb inversion
          3. verb capitalization
          4. subject cannot be object pronoun (me)
          5. some/any/none/all/most substitute object of preposition (of)
          6. passive verb skip
          7. verb after verb cost (if the verb in the pattern is followed with a compatible verb)
          8. bject too far from verb cost
        3. number of verb objects
          1. if verb is passive and we are not in a question, skip
          2. if we are in a question, adjust the objects.
          3. Adjust based on the number of word relations between the verb and its objects.
          4. If objects are too far apart, cancel the word relations adjustment
          5. If an object is 'here' or 'there', remove it from consideration.
          6. Otherwise get the cost of the verb for the expected number of verb objects.
        4. decrease cost based on statistical relations between subject/verb and object
      4. PHASE 2: assess cost again, but preferring verbrel, and using onlyAloneExceptInSubPatternsFlag, accumulating lowest cost
      5. PHASE 3: set winner to all lowest cost positions
    7. Print final (lowest cost) parse depending on debug settings
    8. Compress sentence pattern structures by removing the higher cost patterns from memory
    9. Report unmatched sentences depending on debug settings
    10. Match meta patterns against parsed data
      1. Name equivalence patterns (to understand when a character is being introduced)
      2. Meta announce – the author is explicitly saying whether a character is coming or leaving
      3. Meta speaker – the author is explicitly saying which character is speaking
      4. Meta speaker query and response – characters are explicitly asking and responding about their names or identity
      5. Meta groups – the author is explicitly saying which characters are grouped together
    11. Determine chapter or sections
    12. Update source statistics in DB and print them in log
1. **Read WordNet**
  1. Synonyms
  2. Antonyms
  3. Gendered adjectives
  4. Gendered Nouns
  5. Make adjustments to wordnet data (error correct)
2. **Object Identification (identifyObjects)**

identify object of the lowest cost, correcting parses. Objects are identified either from the pattern or from the word class (proper noun, pronoun, special adverbial pronouns, accusatives). Also flags container objects which contain other objects, assigns relative clauses, assigns object subtypes (OCEAN\_SEA, ISLAND, etc), accumulates adjectives and removes adjectives that are inconsistent (old girl). Also finds the anaphor of the object and whether the object is pleonastic (derived and expanded from Lappin and Leass 2.1.2). Also determine any exact matches.

  1. Create narrator and audience objects
  2. Detect questions
  3. Determine objects by using tags embedded in patterns and attempting to disambiguate when multiple patterns match – this includes adjectives and ownership but not attached prepositional phrases
  4. Detect primary matching word within each object (is it the last noun?)
  5. Determine gender and ownership (Ben's car) as well as disambiguating the objects.
1. **Read Word Relations**
  1. (how many times a word is associated with another through a certain relationship like subject/verb/object/etc)
2. **Analyze Word Sense (analyzeWordSenses)**
  1. Noun
    1. Physical object? (Measurable object)
    2. Psychological? Cognitive? Abstraction?
    3. Which group if any the noun is in
    4. Disease?
    5. Human relation? [husband, wife, etc]
  2. Use VerbNet to associate a verb with a semantic class. This has also been extended to cover most verbs.
  3. Use WordNet to determine noun type like physical object/psychological object/and what set it might belong to like colors.
  4. Determine internal and external body parts and track whether objects should exist physically, denote time, are measurable, etc.
3. **Analyze Syntactic Relations (syntacticRelations)**

    1. adjust noun determiner (evaluateNounDeterminer) – collect statistics on whether a noun has a determiner
    2. adjust verb objects (evaluateVerbObjects) – collect statistics on how many objects a verb has

    1. set probable and lingering statements
    2. Attempt to link lists of nouns together by using types of objects and tags from patterns (markMultipleObjects) – uses mobjectTagSets
  1. Link prepositional phrases together and attempt to determine prepositional phrase attachment (code mark CMREADME015)
  2. Double and single quote matching (CMREADME016)
  3. Update form usages (CMREADME017)
    1. deltaUsagePatterns store the difference in the usage of the word for this source
    2. usagePatterns store the total usages over all sources thus processed
    3. Transfer usagePatterns to usageCosts. This allows a leveling for pattern evaluation, costing between 0 to 4 for unusual form usage.

  1. Determine Role – this fills in the field objectRole with the different contexts a position can be in based on matching matterns. (CMREADME018)
  2. adjust hail role – objects contained within a quote that suggest the speaker is referencing someone who is in the physical environment.
  3. setAdditionalRoleTags –
    1. use subjectVerbRelationTagSet and verbObjectRelationTagSet to fill in remaining roles.
    2. Fill in preposition roles.
    3. determine verb tense and aspect (see Verb Senses table), and use this to fill in roles.
    4. Relate all words with each other (including coordinated nouns, coordinated verbs, within infinitive and relation clauses, and chain prepositional clauses together and with their verbs or nouns)
    5. Connect prepositional clauses that need the processing just concluded above to be connected.

1. **Identify Speaker Groups**

Identifies groups of people who are either speaking or are in the audience (whether covert or not). All resolvable object types are resolved using a procedure derived from the original paper from Lapping and Leass – An algorithm for Pronomial Anaphora Resolution. This object resolution is described in the object resolution section.

  1. Age upon every \_\_S1 pattern. Each object class is aged by a different schedule.
  2. Test for implicit objects – objects that imply other objects ('another knock' means a door must exist)
  3. associateNyms – gather associated nouns and adjectives through an IS relation (as synonyms or antonyms depending on the use of negatives), associate ages
  4. associatePossessions – use has relation (which utilizes verb classes extended from ([English Verb Classes and Alternations: A Preliminary Investigation ](https://www.amazon.com/gp/product/0226475336/ref=ox_sc_act_title_20?smid=A1KIF2Y9A1PQYE&psc=1)by Beth Levin)
  5. setPOVStatus – Using syntactical clues to figure out point-of-view (the narrative is explaining what is going on from which character's point of view)
  6. Resolve object not in a quote (because resolving something in a quote requires knowing who the speaker is), or if the object represents a hail (a person or entity in q uote which is a reference to someone in the physical environment and so therefore is present outside of quote.
  7. Move associated objects if there is only one matched object
  8. Match names based on an explicit syntactic pattern (identifyMetaNameEquivalence)
  9. Use \_META\_SPEAKER patterns to identify explicitly stated speakers (identifyMetaSpeaker).
  10. Use \_META\_ANNOUNCE patterns to identify new physically present people (identifyAnnounce).
  11. Use \_META\_GROUP patterns to identify new physically present people because they are tightly associated with other people who are physically present. (identifyMetaGroup)
  12. Identify object by looking it up in the ontology (identifyISARelation)
  13. Detect meta statements that say what time or date it is explicitly, even a relative time or date (detectTimeTransition)
  14. Define an object as neuter (and non gendered name class) if it is an object of exiting or entering, has a subtype, is a name and is not neuter already (defineObjectAsSpatial)
  15. Collect statistics: how many first and second person objects there are, and how many usages of the past verb tense.
  16. (CMREADME20) Find the first gendered subject in each paragraph. If they are not mergable into whereNextISNarrationSubjects, insert this subject into that vector. Also if there are no matches for a NAME, attempt to merge the name.
  17. (CMREADME21) accumulate groups of gendered objects by syntactic grouping (that and that) or by other syntactic clues (accompanied by) (accumulateGroups).
  18. (CMREADME22) delete from tempSpeakerGroup all objects that are marked eliminated.
  19. (CMREADME23) identify HAIL objects (people who are mentioned in certain patterns indicating that the person who is speaking is addressing a certain other person or group of people). Also remove a hail off of a place, adding hail to a speaker mention next to a quote, and detect if the speaker is talking to himself.
  20. (CMREADME24) if processing a meta data response, and not at end yet, go to the beginning of the processing loop. Also process some special words indicating tables that have been processed from HTML conversion.
  21. (CMREADME25)
    1. Process and link open and closed single and double quotes.
    2. See whether quote is a quoted string (not actually spoken), keeping in mind inserted quotes.
    3. Assign speaker and audience to a double quote by scanning previous and following speakers, and hails
    4. Process if there are multiple quotes in a single sentence.
    5. Remove objects that include quotes which are assigned a speaker.
  22. (CMREADME26) – End Of Sentence (EOS)
    1. Set last S1 (sentence), last relative phrase, or last command encountered to -1.
    2. Set endOfSentence to true, and lastSentenceEnd to the present word position.
    3. Attempt to set a space relation, if there is one. If one is found, age all objects that were between the last space relation found and the present one, using unquoted markers that indicate the last sentence ended.
    4. Determine whether the current sentence is a question, and track what the speaker of the question is, skipping words contained in single quotes. Set the questionSpeaker to the position of the double quote. If the question is in a single quote, just set that it is a question (flags). This is used to determine question agreement later.
    5. If no longer in a single or double quote, then eliminate subjects tracked as available for immediate resolution.
    6. Age objects.
    7. Store last subjects in previous last subjects, and empty last subjects.
    8. If this is NOT the end of a paragraph, and a close quote does not occur immediately at the end of the sentence, and inPrimaryQuote is set, then set quotesSeenSinceLastSentence to true.
  23. (CMREADME27) – end section
    1. Question subject tracking
    2. Logic around creating a speaker group and aging speakers in the speaker group
  24. (CMREADME28) – begin section
    1. Age speakers (again?)
    2. Age preIdentified Speakers
    3. Clear last, narration and unquoted subjects
    4. Create new speaker group
    5. Clear objects and speakers in new section
  25. (CMREADME29)
    1. Delete aliases and replacements from speaker groups. If the speaker group is then reduced to 1 speaker and a self conversation has not been detected, delete the speaker group.
    2. distribute povInSpeakerGroups and definitelyIdentifiedAsSpeakerInSpeakerGroups to povSpeakers and dnSpeakers in speakerGroups.
    3. Clear next section
    4. Print speaker groups to log.

1. **Resolve Speakers**
  1. Reset object associations with other objects, gender associations, all counters associated adjectives, tense associations
  2. For each position in the source:
    1. Meta response (skip response) processing
    2. Evaluate metawhere query
    3. associateNyms – gather associated nouns and adjectives through an IS relation (as synonyms or antonyms depending on the use of negatives), associate ages
    4. associatePossessions – use has relation (which utilizes verb classes extended from ([English Verb Classes and Alternations: A Preliminary Investigation ](https://www.amazon.com/gp/product/0226475336/ref=ox_sc_act_title_20?smid=A1KIF2Y9A1PQYE&psc=1)by Beth Levin)
    5. adjustHailRole
    6. resolveObject
    7. moveNyms
    8. identify meta name equivalence
    9. identify meta speaker
    10. identify announcements
    11. identify meta groups
    12. identify letters (a written message identified in the source, rather than part of the direct narrative, may be 'read' by a character)
    13. age objects if there is a space/time transition in the narrative (outside of quotes).
    14. Identify an ISA relation
    15. (CMREADME30) – give the following an object/subject role: that is a name, has been evaluated and not a HAIL, and in between a conjunction or a quote and the end of the sentence, and has been definitely identified as a speaker. In addition if it appears in the localObjects array, give it an unresolvable from implicit object role as well.
    16. (CMREADME31) – if this is not a question, accumulate as a subject anything with a subject role. However, if this does not have an MPLURAL role, or the previous subject did not have an MPLURAL role, erase the previously accumulated subject. Also set the whereFirstSubjectInParagraph variable if applicable.
    17. (CMREADME32) – clear the local object cache if a table or column (HTML) is encountered.
    18. (CMREADME33) – process quotes (double (primary) and single (secondary) quotes)
      1. Handle embedded story speakers (when speakers tell a story to their audience). This creates an embedded story speaker group inside the current speaker group.
      2. Handles meta speaker query answer, if a query is open
      3. On an end of primary quote, call processEndOfPrimaryQuoteRS:
        1. Analyze whether this is a quoted string
        2. Assign the previously created speakers to the beginning of the quote – the next steps will qualify each object in the audience speakers.
        3. (CMREADME34) scan quote for audience - if supposed audience is included in the quote, then reject
        4. (CMREADME35)
          1. scan the words around the quotes (HAIL\_ROLE AND IN\_QUOTE\_REFERRING\_AUDIENCE\_ROLE)
          2. identify the speakers through the patterns around the quotes.
          3. If there are no speakers identified, try just IN\_QUOTE\_SELF\_REFERRING\_SPEAKER\_ROLE.
          4. resolveMetaReference – specifically recognize "referring to" "27 Carshalton Gardens," said Tuppence, referring to the address.
          5. Match speaker position against previous speakers
          6. Process audiencePosition
          7. If audience is not found and previous quote had an audienceobject where the subject was talking to himself, and speaker is found, then cancel the assumption that the speaker is talking to himself.
          8. resolve the primary link with help from the forwardLink audience if definite (audienceInSubQuote) and only one entity and previous is not one entity and not definitely resolved. Delete the audience if they match the speakers.
        5. (CMREADME36) –
          1. Insert speaker locations for each audience object
          2. Link primary quotes
          3. If the last quote was an embedded quote, make this current quote embedded.
          4. Log the linking.
        6. (CMREADME37) –
          1. Handle embedded speaker group
          2. Set salience aging method
          3. Scan for audience speakers (HAIL and in-quote referral audience role)
          4. Determine speaker position
          5. Shift speaker position according to ownership pattern of speaker
          6. Scan for speakers (in quote self referring speaker role)
          7. If there is more than one speaker, and if the speaker position object is not found in the current speaker group, invalidate speaker position.
        7. (CMREADME38)
          1. If speaker position is found
            1. Check if a referring pattern is used.
            2. Impose speaker
              1. definitelySpeaker is from scanForSpeaker. It is set to true unless the speaker was detected after the quote in an \_S1 pattern with a verb with a nonpast tense, or the verb had one or more objects.
              2. if subjects in previous unquoted section usable for immediate resolution, and last unquoted subjects position exists and is not plural, set last subject preference=true for all local objects outside primary quotes which match the last subjects. last subject preference used in chooseBest
              3. resolve object at speakerObjectAt
              4. if all beforePreviousSpeakers are in the speaker object, then delete all of the beforePreviousSpeakers from the speakerObject position.
              5. also resolve the principal object the speakerObjectAt is related to, if it is a part of an object
              6. remove observers
              7. reset lastSubject preference
              8. are any speakers in the speakerGroup using this as an alias? If so, replace with actual speaker.
              9. resolve audience
              10. set speaker and audience. If the speaker is definitively specified, but the speaker object is a gendered pronoun, for which there is more than one matching gender, then flag as ambiguous.
              11. if speaker/audience is definite and not a pronoun, set the speaker and audience as definite in counters, definitive speaker groups, section speakers, local focus
              12. (CMREADME39) if beginQuote\>=0
                1. resolve previous uncertain speaker matches by alternating backwards
                2. set audience speaker, if there has been an audience position specified.
                3. If not, if the current speaker has no overlap with the last definite speaker, set the audience to the last definite speaker.
                4. If the audience speaker has not been specified and there is no last definite speaker which is different han the curent speaker, then

                1. Get audience speakers from previous speakers, if identified or from most likely speakers.
                2. Extend if audience has been identified by a body part.
                3. Detect an intro

                1. Update previous speakers, and update counts of speaker identification for each speaker object.
            1. Set previous speakers uncertain flag
          1. Else if speaker position is not found
            1. Set embedded story speaker
            2. If there is nothing but a quote (no text before or after the quote), get and impose the most likely speakers.
            3. Match before previous speakers to the audience, if it is set.
        1. (CMREADME40)
          1. Set if quoted string seen
          2. Remove any objects which were set to quotes which have been determined are not quoted strings.
          3. Resolve quoted POV objects
      1. Resolve quoted POV objects for single quotes (secondary quotes)
    1. (CMREADME41) – end of sentence processing - processEndOfSentenceRS
      1. Reset metaSpeakerQuery, last pattern structure variables, other state variables
      2. Process unquoted previous and last sentence ends to scan for space relations and meta where query patterns. Even if sentence is only one word, construct a space location for it.
      3. reset subjectsInPreviousUnquotedSectionUsableForImmediateResolution
      4. use questions to enhance the identification of speakers
      5. age speakers
      6. save subjects and clear current subjects
      7. age embedded speaker groups
      8. set quotesSeenSinceLastSentence.
    2. (CMREADME42)
      1. Process the end of the section
      2. Clear all local objects
      3. Clear endOfSentence, immediatelyAfterEndOfParagraph, etc
      4. If no quotes have been seen since the last sentence and primary quote still open, set continuous quote.
      5. If there are still subjects from unquoted section and unresolved speakers:
        1. If the last pargraph was a question, resolve speaker using speaker inversion.
        2. Then try using previous subjects.
        3. Then resolve using question agreement.
      6. Clear unresolved, previous and before previous speakers.
      7. Set previous subjects in unquoted section.
      8. Reset question variables.
    3. (CMREADME43)
      1. If this is the first section, age speakers if the speaker group does not span the section.
      2. Set up for new section by resetting state variables.
      3. Reset and resort objects in narration, objects spoken about, and speaker objects
      4. Age into a new speaker group (ageIntoNewSpeakerGroup)
  1. (CMREADME44)
    1. Detect space relations
    2. Reset and resort objects in narration, objects spoken about, and speaker objects
    3. Process aliases and replacements in embedded (secondary) speaker groups
    4. Translate all body objects
    5. Make sure object replacement chains don't have any loops.
1. **Resolve First or Second Person Pronouns**
  1. initialize timeline
  2. for each position in the source
    1. create timeline segment
    2. analyze metagroup object (my friend, your friend, friend of mine)
    3. if an object is a HAIL, it matches an object, it is in a primary quote and the primary quote has an audience, AND there is a commonality between the object matches and the audience, remove the audience from the position.
    4. If it is an object, and the object is a HAIL, it is in a secondary quote and the secondary quote has audience matches, remove the audience from the object matches. Also, if the hailed object is not in the secondary quote audience, add it.
    5. If the word is a first or second person (I/you/us/mine), and we are not in a primary or secondary quote, and if a first person is a grouped speaker, add all the others in the group, otherwise add all speakers.
    6. If the object is in a primary or secondary quote, resolve pronoun:
      1. If not a reflexive or pronoun class, erase all pronoun matches, change present object to pronoun class, and mark it not resolved.
      2. If REFLEXIVE\_PRONOUN\_CLASS
        1. If first or second person, match the object to speakers.
        2. If third person, remove current speaker, previous speaker and all current speakers.
      3. If PRONOUN\_CLASS
        1. If first or second person, match the object to speakers.
        2. If third, and not 'one'
          1. If the current speaker group has more than two speakers, and there are either no matches, or the matches are current or previous speakers, push all objects in the current speaker group that are not current or previous speaker
          2. Otherwise, remove current speaker, previous speaker and all current speakers.
      4. If in an IS statement, the subject is a first or second pronoun, not matched and NOT in a probability statement.
        1. If the subject is a first person, and there is no intersection between what is matched and the current speaker, push the current speaker.
        2. If the subject is a second person, and there is no intersection between what is matched and the previous speaker, push the previous speaker.
      5. If (definitely plural, OR (if not first or NOT a definite speaker) AND (not second or NOT definite audience))

AND

Only one current speaker

AND

NOT in a story

AND

First person OR NOT second person OR more than one previous speaker OR this object is not matched

THEN

Match a subgroup

1. **Print Objects**
  1. Print each object and whether it is replaced.
  2. Count total/suspect/ambiguous objects
2. ~~**Resolve Word Relations**~~

During syntactic relations, delayedWordRelations records relations that need speaker resolution before being added. resolveWordRelations takes the speakers that have been resolved with resolveSpeakers and adds them to the word relations for each word. This has been removed because word relations are no longer being dynamically updated regardless of the tests being run. Instead, the word relations will be collected for the entire corpus and then reviewed against the current relations to improve accuracy.

1. **Print Resolution Check**

Prints out object resolution information in rescheck.lplog which is specifically for checking object resolution.

1. **Identify Conversations**

Isolate all conversations for further study – not finished

1. **Write WordNet Maps**

Write wordnet synonyms, antonyms and gender information for nouns and adjectives to a cache file per source

## Question Answering

Most source is contained in the cQuestionAnswering class.

processQuestionSource is the main entry procedure into question answering.

## Processing Stages

1. Prepare question
  1. Find the question type in an object of the current space relation
  2. Detect whether the question is passive or transitory (isQuestionPassive, detectTransitoryAnswer)
2. Log the question
3. Check previous answers (matchAnswersOfPreviousQuestion)
4. Check db for answer (matchOwnershipdbQuery , dbSearchForQuery)
5. If none could be found:
  1. Scan abstract and wikipedia/wikipedia link sources derived from the RDF type (analyzeRDFTypes)
  2. Detect subqueries (detectSubQueries)
  3. Determine best answer (determineBestAnswers)
  4. If no final answer can be found, or there needs to be more answers:
    1. Generate a list of search strings to be used for web queries (getWebSearchQueries)
    2. Keep searching results:
      1. Search 15 results of Google. If any answers, append to main answer list.
      2. Search 15 results of BING. If any answers, append to main answer list.
      3. If this is the first time through the loop, search tables extracted from the wikipedia sources scanned in step 5a.
    3. If no answers are found, for each question information source:
      1. for each question information source object:
        1. sort proximity objects
        2. log them
        3. add the proximity object words to the previously generated web search strings
        4. search Google and BING
    4. For each suggested answer search web.
    5. Insert all the web search answers into the answers from the previous steps.
  5. Log answers (printAnswers)
  6. Find constrained answers (and log them) (findConstrainedAnswers)

## Transformations

**Transformation pattern:**

{ X=explanatory name:form|word,form|word… } {X=profession:noun|job,noun|avocation}

{ X=explanatory name:form … } {A=What:commonProfession }

{ X=explanatory name:pattern } {Y=Danny's:\_NAMEOWNER}

{ X=explanatory name:form|pattern } {Y=NP:noun|NAME\_PRIMARY}

**Transformation flags:**

mapQuestion {SOURCE}

if question maps to this pattern, it will be transformed into the traceTransformDestinationQuestion pattern directly following this flag.

transformDestinationQuestion{DESTINATION}

the transformation of the question. This will be used in place of the SOURCE for pattern matching.

questionPatternMap {DESTINATION}

This matches a pattern named (\_META\_NAME\_EQUIVALENCE in the below example)

{G=PATTERN: **\_META\_NAME\_EQUIVALENCE** } {Y=NP:noun|NAME\_PRIMARY} {A=NS:noun|NAME\_SECONDARY}

linkQuestion {LINK}

1. readWord will detect that a pattern is being parsed because it is a word bounded by curly brackets, an upper case letter after the opening curly bracket (the VARIABLE) and then an = sign (returns PARSE\_PATTERN)
2. This is used in parseBuffer to save the original word in a temporaryPatternBuffer map from position to word. The word is parsed into the VARIABLE and what is after the = sign and placed into the parseVariables map. If there is no = or : then the variable is looked up in the parseVariables map and the value is substituted into sWord.
3. In initializeTransformations:
  1. The questionTransforms file is parsed.
  2. For each sentence:
    1. A pattern is created. For each word in the sentence:
      1. The name is the
    2. If the QuestionPatternMap flag is on, the metaPattern flag of the pattern will be set.
    3. The pattern is additionally accumulated in the patternsForAssignment vector.
    4. If it is a DESTINATION question pattern (see transformation flags)
      1. Print the relation found corresponding to the pattern source location
      2. assign all the patterns in the patternsForAssignment array to the space relation in the transformationPattern map.
      3. Clear the patternsForAssignment array.

## Other important routines

1. analyzeQuestionFromSource is used in steps 5a and 5d. It first accumulates proximity information in accumulateProximityMaps and then processes each relation in a source and compares it by syntactic component to the original question using the sriMatch procedure.
2. accumulateProximityMaps accumulates additional words to use in web search strings to accumulate information to answer question. These additional words come from each child source, based on how often and how close these objects are to the original question information source objects.

## Input sources

1. BNC (getBNC.cpp) – British National Corpus. Its LP purpose was to form word statistics to use for parsing. After translating SGML into ASCII, word class tags into LP class tags and word class preferences (when BNC was ambiguous which class the word belonged to), each word of each sentence of each document within BNC (each document has an id which is recorded in the sources table in the lp MYSQL DB) is read. Word statistics are accumulated (how often a word is a noun, verb, etc) which was used as the bootstrap for the rest of the system.
2. Question search using Search services (getGoogleCustomSearch.cpp) -
  1. Search Google using API returning JSON
  2. Search BING using its API returning JSON
  3. construct a query string for either API using subject, verb, object prepositional phrase
  4. each sentence may result in n number of search strings because of objects having multiple matches. Also adjusts the verb according to its original tense, puts quotations around each object, or around everything or nothing. Prepositional phrase is optional.
3. Interview transcripts (getInterviewTranscripts.cpp) -
  1. Extracts interviews from PBS or NPR. This is for future use to analyze conversation and how to construct a meaningful and related response.
4. MusicBrainz (getMusicBrainz.cpp) – the first database in LP used for answering questions
  1. Recognizes by key words by subject, verb and object and creates queries based on artist, label or release.
5. NewsBank (getNewsBank.cpp) – retrieves news articles from the NewsBank service
  1. Retrieves articles randomly by retrieval number and date
  2. Parses the article for title and translates HTML into plain text.
6. Twitter (getTwitter.cpp) – retrieves twitter entries. Not used as input for anything yet.
7. Wikipedia (getWikipedia.cpp) – processing the parts of a Wikipedia page:

    1. reduction to content
    2. header
    3. tables (two different kinds)
    4. getting the web path –
      1. if used by google or bing search – cleaned by Jericho (because the format is not predictable)
    5. getting dbPedia customized (dbPedia is derived from wikipedia)
    6. readPageWinHTTP is a routine derived from readPage which uses Windows HTTP routines – not used
    7. getRDFTypes – attempt to follow one object through dbPedia, considering:
      1. its base entry
      2. its resources listed in its base entry
      3. its disambiguations listed with each of these derived resources
      4. its disambiguations derived from its base entry
      5. the resources derived from each of the disambiguations derived from its base entry
      6. look up in freebase
    8. readRDFTypes,writeRDFTypes and associated copy procedures read and write all rdf types belonging to an object from and to disk.
    9. getRDFTypesMaster -
    10. get acronyms from acronyms.thefreedictionary.com
    11. assemble ontology from dbpedia, yago, umbel, opengis
    12. get dbPedia abstract, comment and infoPage
    13.
      1. setPreferred - set rdfType preferred for topHierarchyClassIndexes, rdfType preferredUnknownClass for all rdfTypes

used by printIdentity, getObjectRDFTypes, getExtendedRDFTypes, matchBasicElements

eventually by getExtendedRDFTypesMaster

      1. rdfIdentify – for a given string, give back the types. FromWhere is a logging aid, and should be the calling function. TopHierarchyClassIndexes are the indexes into the rdfTypes array for all the rdfTypes matching a top hierarchy class (knownClasses).
      2. LogIdentity – logs a single rdfType.
      3. includeSuperClasses – make sure that the classes for an object include all the ancestor classes all the way to the top of the hierarchy
      4. compressPath
    1. private methods
      1. extractLinkedFreebaseDescription– look up link pattern {L} in properties string, and then look up link as id into MySQL freebaseProperties table. Return the corresponding properties field as a return value (don't change properties input variable). freebaseProperties was populated by reduceLocalFreebase (no longer maintained) in getFreebase.cpp (not included in project anymore), from freebase-rdf-2014-02-09-00-00 file, downloadedfrom[https://developers.google.com/freebase/](https://developers.google.com/freebase/)(deprecated)
        - deprecated freebase will be replaced by wikidata download when enough of the freebase data has been moved over.

      1. findCategoryRank – if a uri is not found in the ontology in memory (because only superclasses have been included in the base ontology), and belongs to the YAGO ontology, access the YAGO ontology on the web and see whether it is a subclass. Push the superClass into the YAGO ontology.
      2. extractResults – access one type of an object dbPedia on the web (see getRDFTypes, which uses this procedure). Derive type, description and ontological position and rank.
      3. readUMBELNS – used to read source\\lists\\umbel\_reference\_concepts\_v100.n3 – umbel has now been replaced by n3 files under umbel downloads
      4. YAGO – an ontology derived from Wikipedia by Max Planck Institute Saarbrücken
      5. UMBEL - Upper Mapping and Binding Exchange Layer – umbel reference concepts
        - Downloaded from [https://github.com/structureddynamics/UMBEL](https://github.com/structureddynamics/UMBEL) - put into lp\umbel\_downloads
      6. fillRanks attempts to derive a combined ranking of a hierarchy.
      7. getAcronyms – get acronyms from acronyms. the freedictionary.com

## Testing

Subsystem test files. These are located in the tests directory.

| Filename | Description |
| --- | --- |
| Agreement | Noun verb agreement |
| date-time-number | Date, time or number identification |
| Lappinl | Lappin and Leass pronoun resolution rules |
| Modification |
 |
| Nameres | Name resolution |
| pattern matching | Matching larger structural patterns |
| Resolution |
 |
| Time | More time identification |
| timeExpressions |
 |
| Usage |
 |
| verb object | Verb object agreement (none, 1 or 2 objects) |

Testing against Stanford POS is done with the corpus analysis project

## Linguistics Studies

## Increasing Accuracy of POS detection

Unknown words in the words table should be minimized to reduce table lookup time and memory, and increase precision of stemming and combination word detection (stems which are unknown were previously allowed, thus polluting the words table and producing inaccurate classes of words that are incorrectly stemmed). The following steps decreased the number of unknown words in the DB by 80%.

Tables used in task:

CREATE TABLE wordFrequencyMemory (

word CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL DEFAULT '',

totalFrequency INT NOT NULL default '0',

unknownFrequency INT NOT NULL default '0',

capitalizedFrequency INT NOT NULL default '0',

allCapsFrequency INT NOT NULL default '0',

lastSourceId INT UNSIGNED DEFAULT NULL, INDEX s\_ind (lastSourceId),

nonEuropeanWord BIT) CHARSET=utf8mb4 COLLATE=utf8mb4\_unicode\_ci ENGINE = MEMORY

The above table must be transferred to the below table to avoid losing data when MySQL resets.

CREATE TABLE wordFrequency (

word CHAR(32) CHARACTER SET utf8mb4 UNIQUE NOT NULL DEFAULT '',

totalFrequency INT NOT NULL default '0',

unknownFrequency INT NOT NULL default '0',

capitalizedFrequency INT NOT NULL default '0',

allCapsFrequency INT NOT NULL default '0',

lastSourceId INT UNSIGNED DEFAULT NULL, INDEX s\_ind (lastSourceId),

nonEuropeanWord BIT) CHARSET=utf8mb4 COLLATE=utf8mb4\_unicode\_ci

1. Collect frequency of all words through the 25000 books contained in Gutenberg.
2. Collect frequency of all unknown words.
3. Delete all words that are unknown (having wordForm=1), having frequency of 0 (not encountered in any Gutenberg book), and no word refers to the word through mainEntryWordId, except itself:

create table temp (select w.id from wordforms wf,words w where wf.formId=1 and w.id=wf.wordId and w.frequency=0 and w.mainEntryWordId=w.id and (select COUNT(\*) from words w2 where w2.mainEntryWordId=w.id)=1)

alter table temp add unique (id)

delete from objectWordMap where wordId in (select id from temp)

delete from wordRelations where fromWordId in (select id from temp) or toWordId in (select id from temp)

delete from words where id in (select id from temp)

delete from wordForms where wordId in (select id from temp)

1. Delete all words that are unknown (having wordForm=1), and where the mainWordEntryIds and all words having that mainWordEntryId is 0:

create table temp (select SUM(frequency) a,mainEntryWordId from words where mainEntryWordId in (select w.id from wordforms wf,words w where wf.formId=1 and w.id=wf.wordId and w.frequency=0 and w.mainEntryWordId=w.id) group by mainentryWordId having a=0)

alter table temp add unique (mainentryWordId)

create table temp2 (select id from words where mainEntryWordId in (select mainEntryWordId from temp))

alter table temp2 add unique (id)

delete from objectWordMap where wordId in (select id from temp2)

delete from wordRelations where fromWordId in (select id from temp2) or toWordId in (select id from temp2)

delete from words where id in (select id from temp2)

delete from wordForms where wordId in (select id from temp2)

drop table temp

drop table temp2

(This is in the solution specials: project CorpusAnalysis). Next, using the ratio of the frequency of unknown words vs the frequency of the capitalized+all caps versions of the same words, we can judge whether the word should be restricted to a proper noun (thus making it known), and the rest looking up to discover its word forms.

1. If frequency of unknown word\>5 and capitalized and ratio of capitalization/total frequency\>0.95,convert to known word with a noun form (proper noun form will be added by flagging with flagAddProperNoun on occurrence).
2. If (1) is not true, then detect if this is a non-European word by translating unicode to a european character set. If this cannot be done, it is concluded that this is a non european word and no more processing will be done (it will remain unknown).
3. If (2) is not true, then look up on dictionary.com. If it is not defined there, then conclude it is unknown and do no more processing.
4. If (3) is not true, then use webster API (restricted to 1000 queries a day). If found on webster, convert word to known.

## The Role of a Relativizer

The relativizer caused particular difficulty in agreement with Stanford, particularly using the word **that.**

Examples (taken from [https://en.wikipedia.org/wiki/Relativizer](https://en.wikipedia.org/wiki/Relativizer)):

1. The Paris **that** I love.
2. Laila told us **that** the actors are on strike.
3. I have friends **that** are moving in together. (subject)
4. **That**'s one thing **that** I actually admire very much in my father. (direct object)
5. Everyone's kinda used to the age group **that** they work with. (object of preposition)
6. It's just kinda something **that** I noticed recently.
7. They get values and stuff like **that** from church **that** they might not get at home.
8. All **that** she wants to do is sleep.
9. She held onto all those jewelry boxes **that** everybody made for her when we were kids.
10. **That**'s the only place **that** you can go at night.
11. **That**'s the first compliment **that** I've got in a long time.
12. **That** was the worst job **that** I ever had.
13. You have a home here **that** you could rent.
14. This pair of suede pants **that** I got.
15. The weight **that** I should be at.
16. I don't think you have the dedicated teacher **that** I had.
17. And it was a guy **that** she worked with for a few years.
18. I have two cats **that** I'd like to turn in to the Humane Society.
19. Do you remember exactly the road **that** I'm talking about?
20. **That** was one of the things **that** he did when he was living elsewhere.
21. I always go to my girlfriends 'cause there's stuff **that** your parents just don't need to know.
22. I wonder what inspired them.
23. I wonder whose dog died.
24. The person who visited Kim.
25. The chairman listened to the student who(m) the professor gave a low grade to.
26. The Pat **that** I like is a genius.
27. The Pat who I like is a genius.
28. The only person **that** I like whose kids Dana is willing to put up with is Pat.
29. Every essay **that** she's written which I've read is on **that** pile.
30. Every essay which she's written **that** I've read is on **that** pile.
31. He has four sons **that** became lawyers.
32. The soldiers who were brave ran forward.
33. He has four sons, who became lawyers.
34. The soldiers, who were brave, ran forward.
35. A yard in which to have a party.
36. The baker in whom to place your trust.

## CoreNLP Comparison:

1. Review conducted using Stanford POS Parser (NOT MaxEnt)
2. Goal was to improve LP parser to match Stanford more closely or understand valid differences
3. A few examples of the differences between Stanford and LP:
  1. LP tags a noun being used as an adjective as a noun word matched into an adjective slot in a NOUN pattern.
  2. Stanford tagger will also tag NAME, in the phrase "said NAME" as a verb. This is correctly parsed by LP as a conversational meta form, with NAME as a proper noun. This case has been filtered out of the different result statistics.
  3. A demonstrative\_determiner at the head of a REL1 pattern will count as a relativizer for LP.
  4. Run is incorrectly characterized as mostly taking one object
  5. Words that can be an adverb or adjective immediately after a verb with no object may be incorrectly characterized as an adjective by LP.
  6. A word that could be an adjective or an adverb is characterized as an adverb by LP when used by a being verb, which is actually an adjective.
  7. Along is incorrectly characterized by LP as an adverb and not a preposition
  8. Here and there are incorrectly marked as nouns by LP when they are adverbs. (Look here!)
  9. Some or any before a noun may be an adjective (LP) but is better described as a determiner (ST).
  10. So and as are particularly singled out for disagreement
  11. That and this may be incorrectly described as an adverb or adjective by LP when they are a determiner.

Based on CoNLL-2012 shared Task as described: [http://conll.cemantix.org/2012/introduction.html](http://conll.cemantix.org/2012/introduction.html)

Definition of terms

In pattern recognition, information retrieval and binary classification,  **precision**  (also called positive predictive value) is the fraction of relevant instances among the retrieved instances, while  **recall**  (also known as sensitivity) is the fraction of relevant instances that have been retrieved over the total amount of relevant instances. (Wikipedia, 2019).

![](RackMultipart20230606-1-cwl8lr_html_7ae37413c094b167.png) ![](RackMultipart20230606-1-cwl8lr_html_45edded370ed41a3.png)

{\displaystyle F=2\cdot {\frac {\mathrm {precision} \cdot \mathrm {recall} }{\mathrm {precision} +\mathrm {recall} }}}

The F1 score, or F-measure, combines precision and recall is the harmonic mean of precision and recall:

Reference from (Clark)

|
 |
 | MUC |
 |
 | B3 |
 |
 | CEAF_φ_4 |
 | CoNLL |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|
 | Prec. | Rec. | _F_1 | Prec. | Rec. | _F_1 | Prec. | Rec. | _F_1 | Avg. _F_1 |
| Fernandes et al. | 75.91 | 65.83 | 70.51 | 65.19 | 51.55 | 57.58 | 57.28 | 50.82 | 53.86 | 60.65 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Bjorkelund and Kuhn¨ | 74.3 | 67.46 | 70.72 | 62.71 | 54.96 | 58.58 | 59.4 | 52.27 | 55.61 | 61.63 |
| Ma et al. | 81.03 | 66.16 | 72.84 | 66.90 | 51.10 | 57.94 | 68.75 | 44.34 | 53.91 | 61.56 |
| Durrett and Klein | 72.61 | 69.91 | 71.24 | 61.18 | 56.43 | 58.71 | 56.17 | 54.23 | 55.18 | 61.71 |
| Clark and Manning | 76.12 | 69.38 | 72.59 | 65.64 | 56.01 | 60.44 | 59.44 | 52.98 | 56.02 | 63.02 |
| Mention Pair Model | 73.84 | 66.93 | 70.22 | 61.12 | 54.14 | 57.41 | 54.44 | 51.31 | 52.83 | 60.15 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Incremental Model | 77.08 | 67.79 | 72.13 | 66.03 | 54.37 | 59.63 | 58.48 | 52.28 | 55.21 | 62.32 |

## Third Party Software

## Customized Paice/Husk stemmer

Rules have been extended with form types, like SINGULAR.

Emphasis has been placed on finding a root which is a word, instead of a linguistic root, because we would like to derive meaning from the stem. Testing on the stemmer is based on the gutenberg corpus (25,000 books), with the most frequent rules getting custom checks. In particular, stemming was changed to require the root word be known.

## BING Search

Tier: F1

Azure control URL: [https://portal.azure.com/#@dscott1414yahoo.onmicrosoft.com/resource/subscriptions/9ad86dd3-3e4f-4829-8baa-adca97034e05/resourcegroups/LPBingSearchResourceGroup/providers/Microsoft.CognitiveServices/accounts/LPBingSearch/overview](https://portal.azure.com/#@dscott1414yahoo.onmicrosoft.com/resource/subscriptions/9ad86dd3-3e4f-4829-8baa-adca97034e05/resourcegroups/LPBingSearchResourceGroup/providers/Microsoft.CognitiveServices/accounts/LPBingSearch/overview)

Username: [dscott1414@yahoo.com](mailto:dscott1414@yahoo.com)

Password: Jsdu4783Jdsh4$\*

Access Key 1: 345820954c834fa08a227260862bbfe5

Access Key 2: c4ad499d9cda4c67b5c9046bdda989c4

Endpoint: [https://lpbingsearch.cognitiveservices.azure.com/bing/v7.0](https://lpbingsearch.cognitiveservices.azure.com/bing/v7.0)

Subscription ID: 9ad86dd3-3e4f-4829-8baa-adca97034e05

Resource Group: LPBingSearchResourceGroup

## WordNet 2.1

Installed at F:\lp\WordNet.

## MySQL DB

The MySQL DB is called lp. It is currently located in M:\ProgramData\MySQL\MySQL Server 8.0. Programs such as mysqlcheck are located in F:\Program Files\MySQL\MySQL Server 8.0\bin. Tables that are not currently used are crossed out. The character set used is UTF8MB4, with a collation of UTF8MB4\_BIN, specifically in words, sources and in the wordfrequency tables. The username is root, and the hostname has been set to '%'. MySQL uses openssl-1.0.2p, located at C:\openssl-1.0.2p.

Building MySQL

      1. Download MySQL from Github
      2. cmake . -G "Visual Studio 15 2017 Win64"
      3. add flags if boost does not exist: -DDOWNLOAD\_BOOST=1
      4. download openssl (only 1.0.2p works at present)
      5. build openssl (will build into C:\OpenSSL-Win32)

C:\GitHub\mysql-server\>cmake . -DOPENSSL\_ROOT\_DIR=C:\openssl-win32 -DOPENSSL\_LIBRARIES=C:\openssl-win32\lib -G "Visual Studio 15 2017 Win64" -DWITH\_BOOST=f:\boost

Libraries and dlls in C:\GitHub\mysql-server\libmysql\RelWithDebInfo

Upgrading MySQL

1. The 8.0 MySQL branch has a missing ordinal. Please continue to use the mysqllib.lib, mysqllib.dll, mysqllib.pdb in the F:\lp\tempMySQL directory, Do not use the packaged versions.
2. In 8.0 the easiest way to restore from another version or a backup for MyISAM files is to save the entire data directory BEFORE installing new version, or at least opting NOT to whack it when uninstalling or avoiding installing over it.
3. The easiest way is to copy the entire data directory and then point the new MySQL installation to the data directory during install. Then edit the my.ini to include a max\_heap\_table\_size=3G. That is it!
4. If in circumstances where the entire data directory is not available or corrupt, here is a few more instructions to load each table individually into a new mysql instance:
  1. Create schema lp;

Use lp;

IMPORT TABLE FROM 'C:\\ProgramData\\MySQL\\MySQL Server 8.0\\Uploads\\*.sdi'

(This depends on the setting of the  [secure\_file\_priv](https://dev.mysql.com/doc/refman/8.0/en/server-system-variables.html#sysvar_secure_file_priv) system variable, which contrary to documentation, must be set. Just remember the double slashes in the above command and this should be ok.)

  1. There may be a problem with the wordrelationsmemory sdi file because the wordrelationsmemory table has no associated MYD file because there is no data. Just delete it and run this command:

CREATE TABLE `wordrelationsmemory` (

`id` int(11) unsigned NOT NULL AUTO\_INCREMENT,

`sourceId` smallint(5) unsigned NOT NULL,

`lastWhere` int(10) unsigned NOT NULL,

`fromWordId` int(10) unsigned NOT NULL DEFAULT '0',

`toWordId` int(10) unsigned NOT NULL DEFAULT '0',

`typeId` smallint(5) unsigned NOT NULL DEFAULT '0',

`totalCount` int(11) NOT NULL DEFAULT '0',

UNIQUE KEY `id` (`id`),

KEY `uw_ind` (`fromWordId`,`typeId`)

) ENGINE=MEMORY AUTO\_INCREMENT=36258188 DEFAULT CHARSET=latin1 DELAY\_KEY\_WRITE=1;

  1. Edit my.ini file to include this, and restart the service.

max\_heap\_table\_size=3G

  1. Then execute:

insert into wordrelationsmemory select id, sourceId, lastWhere, fromWordId, toWordId, typeId, totalCount from wordrelations;

  1. Thesaurus is a non MyISAM table. It has an ibd file. Move the ibd file if it exists in the lp data directory, and save it somewhere else. Recreate the table:

CREATE TABLE thesaurus (

mainEntry CHAR(32) CHARACTER SET utf8 NOT NULL, INDEX me\_ind (mainEntry),

wordType SMALLINT UNSIGNED NOT NULL,

primarySynonyms CHAR(100) CHARACTER SET utf8 NOT NULL,

accumulatedSynonyms TEXT(1000) CHARACTER SET utf8 NOT NULL,

accumulatedAntonyms CHAR(120) CHARACTER SET utf8 NOT NULL,

concepts CHAR(40) CHARACTER SET utf8 NOT NULL) ENGINE=InnoDB ROW\_FORMAT=COMPACT

  1. ALTER TABLE thesaurus DISCARD TABLESPACE;
  2. Copy thesaurus.idb file back.
  3. ALTER TABLE thesaurus IMPORT TABLESPACE;

## Updating Gutenberg

1. Get Gutenberg text files
  1. Update Cygwin – install 64 bit build, include the "rsync" install package (not included by default). Must put destination directory into mount path syntax by prepending /cygdrive to the drive letter. Installed at M:\cygwin64 - M:\cygwin64\bin\rsync.exe

rsync -av --include "\*.txt" --exclude "\*.\*" --del ftp.ibiblio.org::gutenberg /cygdrive/f/lp/Gutenberg/gutenbergMirror

rsync -av --include "\*.txt" --exclude "\*.\*" --del aleph.gutenberg.org::gutenberg /cygdrive/f/lp/Gutenberg/gutenbergMirror

  1. delete all 'old' subdirectories of f:\lp\Gutenberg\gutenbergMirror (searched for old using Explorer, then deleted all of them)
  2. In powershell window, in directory f:\lp\Gutenberg\gutenbergMirror , copy all \*.txt in the subdirectories to the main directory F:\lp\gutenberg:

Get-ChildItem -Path ".\*.txt" -Recurse | Move-Item -Destination "F:\lp\gutenberg\gutenbergMirror"

  1. In powershell window, in the directory F:\lp\Gutenberg\gutenbergMirror, to remove empty directories-

Get-ChildItem -Recurse . | where { $\_.PSISContainer -and @( $\_ | Get-ChildItem ).Count -eq 0 } | Remove-Item

(run several times until the number of directories stops falling)

1. get Catalog
  1. Download latest catalog from [https://www.gutenberg.org/cache/epub/feeds/rdf-files.tar.bz2](https://www.gutenberg.org/cache/epub/feeds/rdf-files.tar.bz2)
  2. Unzip it, to produce rdf-files.tar (still in the Downloads folder), then unzip again to create directory rdf-files with all the rdfs underneath. (60,000 rdfs as of 7.12.2019)
  3. Copy all the rdf's to F:\lp\Gutenberg\gutenbergMirror. Run in path: C:\Users\dscot\_000\downloads\rdf-files.tar\rdf-files:

Get-ChildItem -Path ".\*.rdf" -Recurse | Move-Item -Destination "F:\lp\gutenberg\gutenbergMirror"

1. Run project ProcessGutenbergFromRDFToSQL – this updates the sources table in the mySQL lp database, and also moves and renames the text files to J:\caches\texts
2. Files that should be skipped (marked in sources as start='\*\*SKIP\*\*':
  1. Title having 'poem','poetry','prose', or ('Index' and 'Gutenberg') or 'a play' or plays, or 'a drama' or ' drama' and ' act', or 'dictionary' or '%Widger''s%'
  2. Books having very few words - numWords\<1000 – also removes texts that accompany audio
  3. numKnown/numWords\>0.08 helps to monitor for old books (h), translations containing too many non-English words (h), etc
  4. select etext,numWords,title,numUnknown/numWords from sources where numUnknown/numWords\>0.08 or numWords\<500 order by numUnknown/numWords desc
  5. Books that are part of a summary that has also been included
  6. Books that are lists of illlustrations (Title containing 'A sketch-book')
  7. Books that are indexes or lists
  8. Books that are written in middle or old English or are primarily not in the English language
    1. Everything from Shakespeare, William
    2. La Fontaine, Jean de
    3. Holinshed, Raphael
  9. Books with encoding problems such as 'Tokyo to Tijuana Gabriele Departing America'. I could manually correct them, but then when gutenberg is refreshed they would come back.

Result (3/2/2019) –

90,632 files in 64,023 folders (F:\lp\gutenbergMirror)

58,970 rdf files (F:\lp\Gutenberg catalog)

~25,000 usable sources total after skipping texts noted in 4 (1876 sources skipped)

## Open Library Internet Archive

The list of books is loaded from this site (used to identify the book RDF type):

[https://openlibrary.org/developers/dumps](https://openlibrary.org/developers/dumps)

The titles only are extracted into table openLibraryInternetArchiveBooksDump. This is because the existing ontology does not list as many books as this source.

## Loading DBPedia into Virtuoso

Virtuoso is currently installed into F:\Virtuoso OpenSource 7.20\database.

The following example demonstrates how to upload the DBpedia data sets into Virtuoso using the Bulk Loading Sequence. The current Virtuoso server being used is version 07.20.3217. The version of dbpedia being integrated from [https://wiki.dbpedia.org/develop/datasets](https://wiki.dbpedia.org/develop/datasets), is version 2016-10. When running SPARQL queries in Virtuoso Conductor ISQL ([http://localhost:8890/conductor/isql\_main.vspx?sid=d85da5cc02d89729ddf6efbc57bed19b&realm=virtuoso\_admin](http://localhost:8890/conductor/isql_main.vspx?sid=d85da5cc02d89729ddf6efbc57bed19b&realm=virtuoso_admin)), you must prepend the query with SPARQL, so it must go before even the PREFIX (and not anywhere else).

1. Increase memory used by Virtuosos in .ini file otherwise it will take a long time and produce errors
2. A folder F:\lp\dbpedia\_sets exists and is specified in the [**DirsAllowed**](http://docs.openlinksw.com/virtuoso/databaseadmsrv.html#fp_acliniallowed) ** ** param defined in your virtuoso.ini file.
3. Restart OpenLink Virtuoso service
4. Load the required DBpedia data sets into that folder
  - The latest data sets can be downloaded from the [DBpedia Download](http://wiki.dbpedia.org/Downloads) page. Note the compressed bzip'ed ".bz2" data set files need to be uncompressed first as the bulk loader scripts only supports the auto extraction of gzip'ed ".gz" files.
  - Gz files end up with errors. Try ttl files directly (although this takes much longer)
5. This is the list of ttl files to be loaded (in f:\lp\dbpedia\_downloads, and the rest of dbpedia is in J:\backup\dbpedia 2016-10) is in A-P2 in table Review of sources.

1. long\_abstracts\_wkd\_uris\_en.ttl
2. short\_abstracts\_wkd\_uris\_en.ttl
3. mappingbased\_literals\_en.ttl
4. mappingbased\_objects\_en.ttl
5. specific\_mappingbased\_properties\_en.ttl
6. yago\_taxonomy.ttl
7. yago\_types.ttl
8. instance\_types\_en.ttl
9. instance\_types\_sdtyped\_dbo\_en.ttl
10. instance\_types\_transitive\_en.ttl
11. transitive\_redirects\_en.ttl
12. redirects\_en.ttl
13. disambiguations\_en.ttl
14. interlanguage\_links\_en.ttl
15. dbpedia\_2016-10.nt

1. infobox\_properties\_en.ttl (not loaded – imprecise)
2. infobox\_properties\_mapped\_en.ttl (not loaded – imprecise)
3. infobox\_property\_definitions\_en.ttl (not loaded – imprecise)
4. mappingbased\_objects\_disjoint\_domain\_en.ttl (not loaded – unnecessary)
5. mappingbased\_objects\_disjoint\_range\_en.ttl (not loaded – unnecessary)
6. mappingbased\_objects\_uncleaned\_en.ttl (not loaded – unnecessary)

These above three fields (a,b,d,n) are read by the below query in getDescription in createOntology.cpp.

SPARQL PREFIX foaf: \<http://xmlns.com/foaf/0.1/\>

PREFIX : \<[http://dbpedia.org/resource/](http://dbpedia.org/resource/)\>

SELECT DISTINCT ?v1 ?v2 ?v3 ?v4

WHERE

{

OPTIONAL { :_ **RESOURCE** _ \<http://www.w3.org/2002/07/owl#sameAs\> ?s . }

OPTIONAL { ?s \<http://wikidata.dbpedia.org/ontology/abstract\> ?v1 . }

OPTIONAL { ?s \<http://www.w3.org/2000/01/rdf-schema#comment\> ?v2 . }

OPTIONAL { :_ **RESOURCE** _ foaf:homepage ?v3 . }

OPTIONAL { :_ **RESOURCE** _ foaf:page ?v4 . }

}

The above field (f) is read by the below query in findAnyYAGOSuperClass in createOntology.cpp.

PREFIX rdfs: \<http://www.w3.org/2000/01/rdf-schema#\>

SELECT ?v WHERE

{ {

\<http://dbpedia.org/class/yago/\> rdfs:subClassOf ?v

} }

read directly by readYAGOOntology : yago\_taxonomy.ttl: [http://www.w3.org/2000/01/rdf-schema#subClassOf](http://www.w3.org/2000/01/rdf-schema#subClassOf)

read directly by readYAGOOntology : yago\_type\_links.ttl:[http://www.w3.org/2002/07/owl#equivalentClass](http://www.w3.org/2002/07/owl#equivalentClass)

In extractResults:

The SPARQL keyword a is a shortcut for the common predicate rdf:type (note 1 in SPARQL reference [https://www.w3.org/TR/sparql11-query/](https://www.w3.org/TR/sparql11-query/)), giving the class of a resource, with rdfs:type = \<http://www.w3.org/1999/02/22-rdf-syntax-ns#type\>

PREFIX : \<http://dbpedia.org/resource/\>

PREFIX dbpedia-owl: \<[http://dbpedia.org/ontology/](http://dbpedia.org/ontology/)\>

1. SELECT ?v WHERE { :",object," a ?v}

yago\_types.ttl: \<http://dbpedia.org/resource/Paul\_Krugman\> \<http://www.w3.org/1999/02/22-rdf-syntax-ns#type\> \<http://dbpedia.org/class/yago/Academician109759069\> .

1. SELECT ?v WHERE { :",object," dbpedia-owl:wikiPageRedirects ?v }
2. SELECT ?v WHERE { \<",resources[I],"\> a ?v }
3. SELECT ?v WHERE { \<",resources[I],"\> dbpedia-owl:wikiPageDisambiguates ?v }
4. SELECT ?v WHERE { \<",recursiveResources[J],"\> a ?v }
5. SELECT ?v WHERE { :",object," dbpedia-owl:wikiPageDisambiguates ?v }
6. SELECT ?v WHERE { \<",resources[I],"\> a ?v }

1. For modern Virtuoso servers, do not create rdfloader.sql procedure or the bulk loading script. It is already there.
2. In F:\Virtuoso OpenSource 7.20\bin, execute isql.exe in a command window.
3. Execute ld\_dir.

ld\_dir ('F:\\lp\\dbpedia\_downloads', '\*.ttl', 'http://dbpedia.org');

ld\_dir ('F:\\lp\\dbpedia\_downloads', '\*.nt', 'http://dbpedia.org');

1. Register the graph IRI under which the triples are to be loaded, e.g., "http://dbpedia.org":
  - Note that while this procedure will also work with gzip'ed files, it is important to keep the pattern: \<name\>.\<ext\>.gz, e.g., 'ontology.owl.gz' or ontology.nt.gz
  - Note that if there are other data files in your folder (tmp), then their content will also be loaded into the specified graph.

2. Create a file named  **global.graph**  in the 'F:\\lp\\dbpedia\_downloads' folder, with its entire content being the URI of the desired target graph, e.g.,

http://dbpedia.org

1. Finally, execute the rdf\_loader\_run procedure. This may take some time, depending on the size of the data sets.

SQL\> rdf\_loader\_run ();

Done. -- 100 msec.

1. As a result, the Virtuoso log should contain notification that the loading has completed:

10:21:50 PL LOG: Loader started

10:21:50 PL LOG: No more files to load. Loader has finished

1. Run a checkpoint to commit all transactions to the database.

SQL\> checkpoint;

Done. -- 53 msec.

1. Check loading by using this statement:

select \* from DB.DBA.LOAD\_LIST;

to retry loading, run this statement:

delete from DB.DBA.LOAD\_LIST;

1. To check the inserted triples for the given graph, execute

SQL\> **SPARQL** SELECT COUNT(\*)

FROM \<http://dbpedia.org\>

WHERE

{

?s ?p ?o

} ;

1. Install the [DBpedia](https://virtuoso.openlinksw.com/download/) and [RDF Mappers](https://virtuoso.openlinksw.com/download/) VAD packages, using either the Virtuoso Conductor (use System Admin, Packages tab, at bottom)

2. The Virtuoso-hosted data set can now be explored using a HTML browser, or queried from the SPARQL or Faceted Browser web service endpoints. For example, a description of the resource Bob Marley can be viewed as: http://localhost:8890/page/Bob\_Marley

 ![](RackMultipart20230606-1-cwl8lr_html_f3e5bf99dc5dc3bc.png)

Finding and loading YAGO dataset

From: [https://datahub.io/collections/yago](https://datahub.io/collections/yago)

How to clear dbpedia from Virtuoso:

-- Add here all the graphs we use for a clean update (RDF\_GLOBAL\_RESET deletes them all)

SPARQL CLEAR GRAPH \<http://dbpedia.org/resource/classes#\>;

-- Add here all the graphs we use for a clean update (RDF\_GLOBAL\_RESET deletes them all)

SPARQL CLEAR GRAPH \<http://dbpedia.org\>;

## MySQL DB Tables

The MySQL DB name is lp. Tables that are not currently used are crossed out.

| Table | Fields | Description |
| --- | --- | --- |
| Forms | name |
 |
| Forms | shortName | Used in logging |
| Forms | inflectionsClass | If this is a noun, then the class can be matched like a noun. Forms like 'is' can be treated like a verb. |
| Forms | hasInflections | If this is an open class, then hasInflections will be 1 because the words in that class can be inflected. |
| Forms | isTopLevel | Considered a separator (affects whether a pattern is considered alone) |
| Forms | name |
 |
| Freebaseproperties |
 |
 |
| Multiwordrelations |
 |
 |
| Newsbanksources |
 |
 |
| Objectlocations |
 |
 |
| Objectrelations |
 |
 |
| Objects |
 |
 |
| Objectwordmap |
 |
 |
| Prepphrasemultiwordrelations |
 |
 |
| ~~Relationsflow~~ |
 | attempt to track relations over time – not used |
| Sources |
 | Principal table listing all local location of primary text sources where words have been processed |
| Thesaurus |
 |
 |
| Wiktionarynouns |
 | Helps with agentive nominalization, which is used with text generation. |
| Wordforms | formId | If \>=32750 (patternFormNumOffset), this formId is actually a holder for usages (contained in eUsagePatterns):TRANSFER\_COUNT (8) – the number of times this word has appearedSINGULAR\_NOUN\_HAS\_DETERMINER (9) – if the word's winner form is a noun, and it is singular, and it has a determiner, increment this value.SINGULAR\_NOUN\_HAS\_NO\_DETERMINER (10) - if the word's winner form is a noun, and it is singular, and it does not have a determiner, increment this value.VERB\_HAS\_0\_OBJECTS (11) – if the word's winner form is a verb, and it has no objects, increment this value.VERB\_HAS\_1\_OBJECTS (12 – if the word's winner form is a verb, and it has one object, increment this value.VERB\_HAS\_2\_OBJECTS (13) – if the word's winner form is a verb, and it has 2 objects, increment this value.LOWER\_CASE\_USAGE\_PATTERN (14) – if the word appears in lower case, increment this value.PROPER\_NOUN\_USAGE\_PATTERN (15) – if the word matches a proper noun, increment this value.
A word can have no more than 8 forms associated with it.The formId in the DB is always 1 greater than the same representation in memory – you must subtract one from this field after getting from DB and add one when writing to the DB from memory structures!In memory, this is stored in three structures:
1. In the formsArray static variable storing all forms (\<32750) for all words. Each word has a formsOffset and a count which delineates its particular storage area within this static array.
2. usagePatterns – counts of all forms AND eUsagePatterns
3. usageCosts – computed cost based on the ratio of the highest usage pattern and the current usage pattern (as below) [transferUsagePatternsToCosts]

// for wordId=7139:// count is in usagePatterns, cost is in usageCosts//formId | count | cost form name cost calculation (usageCosts)// 100 | 886 | 0 noun (4\*(886-886))/886==0// 101 | 16 | 3 adjective (4\*(886- 16))/886==3// 102 | 418 | 2 verb (4\*(886-418))/886==2
 |
| Wordrelations |
 |
 |
| Wordrelationsmemory |
 | The table wordrelations copied into memory for speed – used for read only |
| Wordrelationstype |
 |
 |
| Words | inflectionFlags | **NOUNS:** SINGULAR=1,PLURAL=2SINGULAR\_OWNER=4,PLURAL\_OWNER=8, **VERBS:** VERB\_PAST=16, VERB\_PAST\_PARTICIPLE=32,VERB\_PRESENT\_PARTICIPLE=64,VERB\_PRESENT\_THIRD\_SINGULAR=128,VERB\_PRESENT\_FIRST\_SINGULAR=256, VERB\_PAST\_THIRD\_SINGULAR=512,VERB\_PAST\_PLURAL=1024,VERB\_PRESENT\_PLURAL=2048,VERB\_PRESENT\_SECOND\_SINGULAR=4096, **ADJECTIVES:** ADJECTIVE\_NORMATIVE=8192,ADJECTIVE\_COMPARATIVE=16384,ADJECTIVE\_SUPERLATIVE=32768, **ADVERBS:** ADVERB\_NORMATIVE=65536,ADVERB\_COMPARATIVE=131072,ADVERB\_SUPERLATIVE=262144, **GENDERS:** MALE\_GENDER=\_MIL\*1,FEMALE\_GENDER=\_MIL\*2,NEUTER\_GENDER=\_MIL\*4,MALE\_GENDER\_ONLY\_CAPITALIZED=\_MIL\*512,FEMALE\_GENDER\_ONLY\_CAPITALIZED=\_MIL\*1024,ONLY\_CAPITALIZED=(MALE\_GENDER\_ONLY\_CAPITALIZED|FEMALE\_GENDER\_ONLY\_CAPITALIZED) **PERSON PERSPECTIVE:** FIRST\_PERSON=\_MIL\*8 /\*ME\*/,SECOND\_PERSON=\_MIL\*16 /\*YOU\*/,THIRD\_PERSON=\_MIL\*32, /\*THEM\*/ **OWNER:** NO\_OWNER=\_MIL\*64,VERB\_NO\_PAST=\_MIL\*64, // special case to match only nouns and Proper Nouns that do not have 's / and verbs that should only be past participles
OPEN\_INFLECTION=\_MIL\*128,CLOSE\_INFLECTION=\_MIL\*256, // overlap
 |
| Words | flags | topLevelSeparator=1, ignoreFlag=2, queryOnLowerCase=4, queryOnAnyAppearance=8, updateMainInfo=32, updateMainEntry=64,insertNewForms=128, isMainEntry=256, intersectionGroup=512, wordIndexRead=1024, wordRelationsRefreshed=1024, newWordFlag=2048, inSourceFlag=4096, alreadyTaken=8192\*256, physicalObjectByWN=8192\*512, notPhysicalObjectByWN=8192\*1024,uncertainPhysicalObjectByWN=notPhysicalObjectByWN\<\<1,genericGenderIgnoreMatch=uncertainPhysicalObjectByWN\<\<1,prepMoveType=genericGenderIgnoreMatch\<\<1,genericAgeGender=prepMoveType\<\<1,stateVerb=genericAgeGender\<\<1,possibleStateVerb=stateVerb\<\<1,mainEntryErrorNoted=possibleStateVerb\<\<1,lastWordFlag=mainEntryErrorNoted\<\<1 |
| Words | timeFlags | all groups of words that refer almost exclusively to time.1. specific time or a specific relative time2. time order (one event happening before another)3. time direction (one event happening in the past, is happening now or will happen in the future)4. recurring time5. Points: ON\_OR\_BEFORE, ON\_OR\_AFTER6. Durations: EQUAL\_OR\_LESS, EQUAL\_OR\_MORE7. Sequential: T\_ASSUME\_SEQUENTIAL=0
The following flags cannot be combined, only one may be active at a timeT\_BEFORE, T\_AFTER, T\_PRESENT, T\_THROUGHOUT, T\_RECURRING, T\_AT, T\_MIDWAY,T\_IN, T\_ON, T\_INTERVAL, T\_START, T\_STOP, T\_RESUME, T\_FINISH, T\_RANGE, T\_META\_RELATION, T\_UNIT, T\_MODIFIER
The following flags can be combinedT\_TIME=32, T\_DATE=64, T\_TIMEDATE=T\_TIME+T\_DATE,T\_VAGUE=128, T\_LENGTH=256, T\_CARDTIME=512 |
| Words | mainEntryWordId | The index of the root of the word |
| Words | derivationRules | These rules are stored in source\\lists\\suffixRules.txt and source\\lists\\prefixRules.txt. Derived and extended from Paice/Husk stemmer. Suffix rule numbers are by line. Prefix rule numbers are -line. Trail derivationRules is computed by rotating by 8 bits and adding the rule number. |
| Words | sourceId | sourceId at which word was last seen |

## Solr Performance Comparison

This was done for testing, comparing MySQL to Solr performance for a simple word relations lookup. This was done using Solr 8.0, and MySQL 8.1. Setup steps are below. The results of 25M rows total. Each run was with a different query, as Solr will completely cache it.

| Seconds | MySQL database | Solr |
| --- | --- | --- |
| Call to retrieve data only | 0023 (first query) 0026 (second) | 0339 (first query) 0364 (second) |
| Creating command to retrieve data, retrieving the data, extracting results and inserting into memory structures | 0035 (first query) 0034 (second) | 0720 (first query) 0844 (second) |

How the wordrelations Solr core was created -

1. Start solr –

F:\lp\solr-8.0.0\> bin\solr.cmd start

1. Build a schema-less core:./Solr create -c WordRelations
2. Create DIHConfigfile.xml, put in the core conf directory (F:\lp\solr-8.0.0\server\solr\WordRelations\conf):

\<dataConfig\>

\<dataSource type="JdbcDataSource" driver="com.mysql.cj.jdbc.Driver" url="jdbc:mysql://localhost:3306/lp" user="root" password="byron0"/\>

\<document\>

\<entity name="wordrelations"

query="select \* from wordrelations"

deltaImportQuery="SELECT id,sourceId,lastWhere,fromWordId,toWordId,typeId,totalCount,ts from products WHERE id='${dih.delta.id}'"

deltaQuery="select id from item where ts \> '${dataimporter.last\_index\_time}'"\>

\<field column="id" name="id" /\>

\<field column="sourceId" name="sourceId" /\>

\<field column="lastWhere" name="lastWhere" /\>

\<field column="fromWordId" name="fromWordId" /\>

\<field column="toWordId" name="toWordId" /\>

\<field column="typeId" name="typeId" /\>

\<field column="totalCount" name="totalCount" /\>

\<field column="ts" name="ts" /\>

\</entity\>

\</document\>

\</dataConfig\>

1. Create these fields in the managed schema:

\<field name="fromWordId" type="pint" uninvertible="true" indexed="true" stored="true"/\>

\<field name="id" type="string" multiValued="false" indexed="true" required="true" stored="true"/\>

\<field name="lastWhere" type="pint" uninvertible="false" default="-1" indexed="false" required="true" stored="true"/\>

\<field name="sourceId" type="pint" uninvertible="false" indexed="false" stored="true"/\>

\<field name="toWordId" type="pint" uninvertible="true" indexed="true" stored="true"/\>

\<field name="totalCount" type="pint" uninvertible="false" indexed="false" stored="true"/\>

\<field name="ts" type="pdate" uninvertible="false" indexed="false" stored="true"/\>

\<field name="typeId" type="pint" uninvertible="true" indexed="true" stored="true"/\>

1. Put these lines in the solrconfig.xml file:

\<lib dir="../../../contrib/dataimporthandler/lib" regex=".\*\.jar" /\>

\<lib dir="../../../dist/" regex="solr-dataimporthandler-.\*\.jar" /\>

\<requestHandler name="/dataimport" class="org.apache.solr.handler.dataimport.DataImportHandler"\>

\<lst name="defaults"\>

\<str name="config"\>data-config.xml\</str\>

\</lst\>

\</requestHandler\>

Make sure that 'dist' folder contains these two files for the data import handler:

solr-dataimporthandler-4.10.2.jar

solr-dataimporthandler-extras-4.10.2.jar

1. In the Solr interface, in the WordRelations core, in the dataimport section ([http://localhost:8983/solr/#/WordRelations/dataimport//dataimport](http://localhost:8983/solr/#/WordRelations/dataimport//dataimport)) and click execute.

## Appendix

## Command Line

(blue parameters must be the first parameter)

| Parameter | Description |
| --- | --- |
| -tg | test gutenberg |
| -acquireNewsBank | acquire more news from the news bank web site |
| -acquireMovieList | acquire more transcripts of movies from a movie script site |
| -acquireInterviewTranscript | acquire interviews from NPR |
| -acquireTwitter | get twitters (free) |
| -server [host] | set database server (MySQL) |
| -log [multiprocessor suffix] | set a suffix for all log file types to distinguish them from single processor operation |
| -mp [0 default] | how many simultaneous parsing processes should occur, does not apply to question answering |
| -numSourceLimit [0 default indicates no limit] | how many sources to process beFore quitting |
| -cacheDir [J:\caches default] | where texts, websearch, wikidata, wikipedia, wordnet, dbpedia and acronyms are cached |
| -resetAllSource | set all sources as unprocessed and not in the processing stage |
| -resetProcessingFlags | set all sources as not in the processing stage |
| -generateFormStatistics | log form statistics |
| -retry | reset all the source that is to be processed determined by the other parameters |
| -parseOnly | parse only, do not do any speaker group evaluation or other higher level processing |
| -logCache | determine number of seconds before flushing logs |
| -bandwidthControl | minimum seconds between internet requests |
| -flipTMSOverride | toggle flag to log all matched sentences (sentences that have a fully enclosing parse) |
| -flipTUMSOverride | toggle flag to log all unmatched sentences (sentences that do not have a fully enclosing parse) |
| -flipTOROverride | toggle object resolution logging |
| -TSROverride | toggle speaker resolution logging |
| -flipTNROverride | toggle name resolution logging |
| -sourceRead | force a source to be read and reparsed from disk even though it has a parse cached file |
| -sourceWrite | will not write a read and parsed source to its cache file |
| -sourceWordNetRead | will not read the wordnet map cache file if false |
| -sourceWordNetWrite | will not write the wordnet cap cache file if false |
| -C | will not read or write a source cache file or a WordNet cache file |
| -Test [#start] [#end or +] | parses and does resolve for speaker, objects, pronouns etc with the following parameter as a test file (only) |
| -Book [#start] [#end or +] | parse the gutenberg books (sourceType 2) |
| -Newsbank [#start] [#end or +] | parse 6GB of news in the lp\NewsBank directory. getNewsbank does not work anymore. |
| -BNC [#start] [#end or +] | British National Corpus – used for cross checking because it is already tagged |
| -Script [#start] [#end or +] | movie scripts |
| -WebSearch [#start] [#end or +] | used for processing bits of information collected for question answering |
| -Wikipedia [#start] [#end or +] | used to process wikipedia articles for question processsing |
| -Interactive [#start] [#end or +] | used for main question answering session |

## Object Roles

| **Object Role** | **Description** |
| --- | --- |
| sentence in alt rel | A sentence pattern is embedded in a relative clause that does not start with an interrogative pronoun (whatever, whenever, wherever, which, whichever, who, whoever, whom, whomever, what) nor whose, where or that. Used to determine a physically present position |
| in command object | an object in a command phrase (\_COMMAND1 pattern) |
| think enclosing | an object in a phrase which is a think phrase (I thought that she was Presbyterian) |
| not enclosing | object is contained in a relative clause that has a negation in it (never, not, etc) |
| extended enclosing | the object is contained in a relative clause with a verb that has an extended tense |
| nonpast enclosing | the object is contained in a relative clause with a verb that does not have a past component to its tense |
| nonpresent enclosing | the object is contained in a relative clause with a verb that does not have a past component to its tense |
| possible enclosing | the object is contained in a relative clause with a verb that has a 'possible' component to its tense (VT\_POSSIBLE) |
| no pp prep | no physical presence preposition – 'of', 'for' or 'from' - the photograph of 'Jane Finn' |
| in quote referring audience | an object in a quote when someone is referring to their audience. "[st:tommy] Mr . Hersheimmer -- Mr . Beresford -- Dr . Roylance . How is the patient[patient] ? " |
| pp object | physical presence – note for all objects tagged with NAME\_PRIMARY, and also having an object class of NAME, GENDERED\_GENERAL, BODY, GENDERED\_OCCUPATIONAL\_ROLE\_ACTIVITY, GENDERED\_DEMONYM, META\_GROUP or GENDERED\_RELATIVE. |
| extended object | the object is in a clause with a verb that has an extended sense (see verb senses below), in a primary S1 or a relative clause. |
| in embedded story object | this object is in a story that one character is relating to another within the book, an embedded narrative. (CMREADME019) |
| in secondary quote | in between secondary quotes that constitute a character saying something (rather than quotes used for other purposes). Secondary quotes are contained in primary quotes, designated by single quote marks ', that are enforced (rewritten) by other routines |
| not object | derived from the notTagSet, any object in a sentence that is denoted as a negation by use of not, never, or otherwise, including prepositional objects |
| in primary quote | in between primary quotes that constitute a character saying something (rather than quotes used for other purposes). Primary quotes are double quotes " |
| focus evaluated | a role has been set for this object |
| delayed receiver | who someone is writing to |
| primary speaker | speaker of a primary quote |
| secondary speaker | speaker of a secondary quote |
| mnoun | an object in an MNOUN pattern |
| pov object | used in determining point of view/observer status for speakerGroups |
| passive subject | a subject having a verb in a passive voice |
| sentence in rel | a sentence contained in a relative clause |
| unresolvable from implicit object | keep this object from being matched with an implicit object. |
| in quote self referring speaker | mark speakers referring to themselves within quotes |
| subject pleonastic | A subject that exists simply for syntactic reasons (it) |
| non movement prep object | An object of a preposition which is linked syntactically to a verb which IS NOT associated with physical movement. |
| movement prep object | An object of a preposition which is linked syntactically to a verb which IS associated with physical movement. |
| place object | an object which has been identified with a place which is either a certain known geographic place name or a geographic type such as a town or region. |
| nonpresent object | an object which is syntactically linked (like a subject or object) with a verb which has a tense which indicates it is not presently physically existing in the environment. |
| is adj object | an object which is an adjective, rather than a noun (so therefore not representing something in the environment) |
| no alt res speaker | restrict speakers - if previousParagraph is set, then don't set NO\_ALT\_RES\_SPEAKER\_ROLE because this is used to ignore this object for subject purposes, which is used to set the speakers of subsequent paragraphs. In the cases of NO\_ALT\_RES\_SPEAKER\_ROLE, we want to ignore it, but in the case of previousParagraph, the object sets the speaker of the NEXT paragraph, so this is what we want. These should this be considered a subject of a sentence to be used as an alternative resolution speaker? if it should NOT, set to NO\_ALT\_RES\_SPEAKER\_ROLE. |
| id sentence type | set at sentence beginning if containing an "is" verb |
| nonpast object | the object is linked to a verb which does not in the past tense. Used to determine speaker status |
| is object | the object is linked to an IS verb. Used to determine speaker status |
| re object | the object is a restatement of an immediately preceding noun phraseThe man, a tennis instructor, was an excellent server |
| prep object | the object is an object of a preposition. |
| iobject | an object within an infinitive clause. |
| hail | speakers referred to in a quote from someone else |
| mplural | multiple subjects & objects |
| meta name equivalence | uses META patterns to determine name equivalence. See createMetaNameEquivalencePatterns.Miss Prudence Cowley, fifth daughter of Archdeacon Cowley of Little Missendell, Suffolk. |
| object | this is an object of a verb, as opposed to a subject. |
| subject | this is a subject of a verb, as opposed to an object. |
| subobject | marked by a SUBOBJECT tag in a pattern – an object which should be recognized but should not be included in calculations for subject-verb agreement or verb objects. |
| no role |
 |

## Object Classes

| **Object Class** | **Description** | **Aging** |
| --- | --- | --- |
| Pronoun | both, either, neither, all, other, less etc | 3 |
| Reflexive pronoun | myself, yourself, etc | 3 |
| Reciprocal pronoun | each other, one another | 3 |
| Name | based on name patterns, includes expected name gender (post office girls and boys names and nicknames) as well as honorific gender (Mr. Mrs. Queen, etc) | 50 |
| Gendered general | determine gender from any source other than relative or demonym | 10 |
| Body | part of body | 3 |
| Gendered occ\_role\_activity | occupation=plumber;role=leader;activity=runner | 10 |
| Gendered demonym | Italian/American/Ohioan | 10 |
| Nongendered general | not in any other category | 3 |
| Nongendered business | can be identified as a business name through usage of incorporated, corporation, company etc or their abbreviations | 3 |
| Pleonastic | pleonastic it as defined in Lappin and Leass 2.1.2 | 3 |
| Verb | the news I brought / running -- objects that contain verbs | 3 |
| Nongendered name | names for which gender cannot be determined | 3 |
| Metagroup | the first, the last, the other, the companion, ally, etc | 10 |
| Gendered relative | uncle, aunt, sister etc | 10 |

## Object Subtypes

| **Object Subtype** | **Description/Examples** |
| --- | --- |
| CANADIAN\_PROVINCE\_CITY | Boucherville |
| COUNTRY | Canada |
| ISLAND | Anticosti Island |
| MOUNTAIN\_RANGE\_PEAK\_LANDFORM | Appalachian Mountains |
| OCEAN\_SEA | Bengal, Bay of |
| PARK\_MONUMENT | Akagera National Park |
| REGION | Algarve |
| RIVER\_LAKE\_WATERWAY | Angel Falls |
| US\_CITY\_TOWN\_VILLAGE | Alexandria (Louisiana) |
| US\_STATE\_TERRITORY\_REGION | Colorado |
| WORLD\_CITY\_TOWN\_VILLAGE | Abéché |
| GEOGRAPHICAL\_NATURAL\_FEATURE | lake mountain stream river pool air sea land space water |
| GEOGRAPHICAL\_URBAN\_FEATURE | city, town, suburbs, park, dam, buildings - definitely a different location |
| GEOGRAPHICAL\_URBAN\_SUBFEATURE | rooms within buildings - perhaps a different location (see spaceRelations) |
| GEOGRAPHICAL\_URBAN\_SUBSUBFEATURE | commonly interacted things within rooms - door, table, chair, desk - not a different location |
| TRAVEL | trip, journey, road, street, trail |
| MOVING | train, plane, automobile |
| MOVING\_NATURAL | elephant, horse, \_NAME (human) |
| RELATIVE\_DIRECTION | front, rear, side, edge, behind |
| ABSOLUTE\_DIRECTION | North, South, East, West |
| BY\_ACTIVITY | lunch, dinner, breakfast, party, wedding |
| UNKNOWN\_PLACE\_SUBTYPE |
 |
| NOT\_A\_PLACE |
 |

## Verb Senses

COERCE guide: may [70% possible, 20% COERCION], could [60/40], should [40/60], must[10/90], do[20/80]

SIMPLE EXTENDEDPASSIVEPOSSIBLE/COERCE

| TAG | EXAMPLE | FLAG | TIME DERIVATION |
| --- | --- | --- | --- |
| vS | examine | VT\_PRESENT | R=E=S |
| vS+past | examined | VT\_PAST | R=E\<S |
| vB | has examined | VT\_PRESENT\_PERFECT | E\<S=R |
| vrB | having examined | VT\_PRESENT\_PERFECT+VT\_VERB\_CLAUSE | E\<S=R |
| vB+past | had examined | VT\_PAST\_PERFECT | E\<R\<S |
| vS+future | will examine | VT\_FUTURE | S=R\<E |
| vAB+future | will have examined | VT\_FUTURE\_PERFECT | S\<E\<R |
| vC | is examining | VT\_EXTENDED+ VT\_PRESENT | S=R E\<=\>R |
| vE (non-Quirk) | naked progressive spreading | VT\_EXTENDED+ VT\_PRESENT+VT\_VERB\_CLAUSE | S=R E\<=\>R |
| vC+past | was examining | VT\_EXTENDED+ VT\_PAST | R=E\<S |
| vBC | has been examining | VT\_EXTENDED+ VT\_PRESENT\_PERFECT | E\<S=R |
| vrBC | having been examining | VT\_EXTENDED+ VT\_PRESENT\_PERFECT+VT\_VERB\_CLAUSE | E\<S=R |
| vBC+past | had been examining | VT\_EXTENDED+ VT\_PAST\_PERFECT | E\<R\<S |
| vAC+future | will be examining | VT\_EXTENDED+ VT\_FUTURE | S=R\<E |
| vABC+future | will have been examining | VT\_EXTENDED+ VT\_FUTURE\_PERFECT | S\<E\<R |
| vD | is examined | VT\_PASSIVE+ VT\_PRESENT |
 |
| vrD | being examined | VT\_PASSIVE+ VT\_PRESENT+VT\_EXTENDED+VT\_VERB\_CLAUSE |
 |
| vD+past | was examined | VT\_PASSIVE+ VT\_PAST |
 |
| vBD | has been examined | VT\_PASSIVE+ VT\_PRESENT\_PERFECT |
 |
| vrBD | having been examined | VT\_PASSIVE+ VT\_PRESENT\_PERFECT+VT\_VERB\_CLAUSE |
 |
| vBD+past | had been examined | VT\_PASSIVE+ VT\_PAST\_PERFECT |
 |
| vCD | is being examined | VT\_PASSIVE+ VT\_PRESENT+VT\_EXTENDED |
 |
| vCD+past | was being examined | VT\_PASSIVE+ VT\_PAST+VT\_EXTENDED |
 |
| vBCD | has been being examined | VT\_PASSIVE+ VT\_PRESENT\_PERFECT+VT\_EXTENDED |
 |
| vBCD+past | had been being examined | VT\_PASSIVE+ VT\_PAST\_PERFECT+VT\_EXTENDED |
 |
| vAD+future | will be examined | VT\_PASSIVE+ VT\_FUTURE |
 |
| vACD+future | will be being examined | VT\_PASSIVE+ VT\_FUTURE+VT\_EXTENDED |
 |
| vABD+future | will have been examined | VT\_PASSIVE+ VT\_FUTURE\_PERFECT |
 |
| vABCD+future | will have been being examined | VT\_PASSIVE+ VT\_FUTURE\_PERFECT+VT\_EXTENDED |
 |
| vS+conditional | may examine | VT\_POSSIBLE+ VT\_PRESENT |
 |
| vS+imp | do examine | VT\_IMPERATIVE+ VT\_PRESENT |
 |
| vS+imp+past | did examine | VT\_IMPERATIVE+ VT\_PAST |
 |
| vS | must examine | VT\_POSSIBLE+ VT\_PRESENT |
 |
| vACD+conditional | may be being examined | VT\_POSSIBLE+ VT\_PRESENT+VT\_PASSIVE |
 |
| vAC+conditional | may be examiningdo be examining | VT\_POSSIBLE+ VT\_PRESENT+VT\_EXTENDED |
 |
| vAB+conditional | may have examined | VT\_POSSIBLE+ VT\_PAST |
 |
| vABD+conditional | may have been examined | VT\_POSSIBLE+ VT\_PAST+VT\_PASSIVE |
 |
| vABCD+conditional | may have been being examined | VT\_POSSIBLE+ VT\_PAST+VT\_PASSIVE+VT\_EXTENDED |
 |
| vAD+conditional | may be examined | VT\_POSSIBLE+ VT\_PRESENT\_PERFECT+VT\_PASSIVE |
 |
| vABC+conditional | may have been examining | VT\_POSSIBLE+ VT\_PRESENT\_PERFECT+VT\_EXTENDED |
 |

## Extended Verb Classes

The broad classes/attributes is an attempt at gathering up the VerbNet classes into larger classes, and making them specifically useful for narrative parsing.

| **Broad Class/Attribute Name** | **Description** | **Examples** | **stFlag** |
| --- | --- | --- | --- |
| Establish | a location is being set for a subject or object | Base, establish, ground, build | stESTAB |
| No\_physical\_action | The action cannot be seen | Admire, appear, care, cheat |
 |
| Control | The subject is trying to control an object | Beg, force, order, subjugate | stCONTROL |
| Contact | two or more objects are in one place or next to each other (because they contacted each other) | Bump, cling, hit, hold, wipe | stCONTACT |
| Near | two or more objects are near one another (one is following the other) | Chase, follow, shadow, tail | stNEAR |
| Locationobject | This indicates location can be an object |
 |
 |
| Locationprepobject | location can be a prepositional object |
 |
 |
| Transfer | Transfering something between things | Bring, contribute, send | stTRANSFER |
| No\_prep\_to | Does not take a binding to preposition | Try, attempt, intend, possess |
 |
| No\_prep\_from | Does not take a binding from preposition | Choke, drown, starve, try |
 |
| Move | an object is moving itself from one location to another | Run, meander, roll, slide | stMOVE |
| Move\_object | an object is moving something or someone | Banish, obtain, give | stMOVE\_OBJECT |
| Move\_in\_place | an object is moving but staying in place (turning, twisting or so forth) | Bulge, coil, curtsey, breathe | stMOVE\_IN\_PLACE |
| Exit | something is exiting from the sight or knowledge of the entity with point of view (important for speaker recognition) | Disappear, leave, withdraw | stEXIT |
| Enter | something is entering the scene (also important for speaker recognition) | Burst, awaken, take shape, emanate | stENTER |
| Contiguous | Something touches something else | Bestride, hug, straddle | stCONTIGUOUS |
| Start | A process starts | Begin, commence, proceed | stSTART |
| Stay | something is NOT exiting but is specifically staying in one place | Linger, stop, sustain, keep | stSTAY |
| Has | The have relation | Have, own, possess | stHAVE |
| Communicate | Communicating in any way | Chit-chat, declare, lecture | stCOMMUNICATE |
| Think | Any verb indicating internal thought | Understand, grasp, fathom | stTHINK |
| Think\_object | Internal thought requiring an object | Esteem, respect, worship | stTHINKOBJECT |
| Sense | Any verb relating to the senses | Hear, see, feel smell, taste | stSENSE |
| Create | Relating to the creative process | Dance, sing, sweat | stCREATE |
| Consume | Relating to eating | Devour, consume, gobble, eat | stCONSUME |
| Change\_state | Changing appearance | Manicure, bulge, slash, grow | stCHANGE\_STATE |
| Agent\_change\_object\_internal\_state | Changing the state of another object | Tickle, upset, stun, annoy | stAGENTCHANGEOBJECTINTERNALSTATE |
| Meta\_profession | Implicitly tell someone's profession | Appoint, ordain, nominate | stMETAPROFESSION |
| Meta\_future\_have | Implicitly tell what someone will have | Bequeath, offer, cede, award | stMETAFUTUREHAVE |
| Meta\_future\_contact | Possibility of future contact (search) | Dredge, excavate, scout | stMETAFUTURECONTACT |
| Meta\_info | Where something might be or happen | Occur, transpire, dwell | stMETAINFO |
| Meta\_if\_then | Future action | Need, require, demand | stMETAIFTHEN |
| Meta\_contains | What something may carry or contain | Store, carry, contain, fit | stMETACONTAINS |
| Meta\_desire | What someone wants to happen or possess | Crave, hunger, sanction, let | stMETADESIRE |
| Meta\_role | Implicit occupation | Police, referee, tailor, tutor | stMETAROLE |
| Meta\_belief | The action of belief | Interview, infer, prove, examine | stMETABELIEF |
| Spatial\_orientation | What surrounds something | Escort, couple, congregate | stSPATIALORIENTATION |
| Ignore |
 | Cost, net, take, carry | stIGNORE |

## Ontology

The ontology is from sources derived or based upon Wikipedia. It is used by identifying an object by matching the name to a category. This category may also have a description. Review of sources (OLD sources will be marked with this color). In the future this may include all of YAGO ([https://www.mpi-inf.mpg.de/departments/databases-and-information-systems/research/yago-naga/yago/downloads/](https://www.mpi-inf.mpg.de/departments/databases-and-information-systems/research/yago-naga/yago/downloads/)) instead of just the YAGO related to dbpedia. Based on pulling tuples in SPARQL on virtuoso opensource (2017). Load virtuoso with dbpedia.org tuples (now upgraded to 2016 edition).

|
 | FILE | RELATION | ONLY | PROCEDURE | METHOD |
| --- | --- | --- | --- | --- | --- |
| A | long\_abstracts\_wkd\_uris\_en.ttl | \<http://dbpedia.org/ontology/abstract\> | X | getDescription | SPARQL |
| B | short\_abstracts\_wkd\_uris\_en.ttl | \<http://www.w3.org/2000/01/rdf-schema#comment\> | X | getDescription | SPARQL |
| C | mappingbased\_literals\_en.ttl | \<http://xmlns.com/foaf/0.1/page\> (foaf:page) | part |
 | SPARQL |
| D | mappingbased\_objects\_en.ttl | \<http://xmlns.com/foaf/0.1/page\> (foaf:page) | part | getDescription | SPARQL |
| E | specific\_mappingbased\_properties\_en.ttl |
 | part |
 | SPARQL |
| F | yago\_taxonomy.ttl | \<[http://www.w3.org/2000/01/rdf-schema#subClassOf](http://www.w3.org/2000/01/rdf-schema#subClassOf)\> | X | findAnyYAGOSuperClass | SPARQL |
| G | yago\_types.ttl | \<[http://www.w3.org/1999/02/22-rdf-syntax-ns#type](http://www.w3.org/1999/02/22-rdf-syntax-ns#type)\> |
 | extractResults | SPARQL |
| H | instance\_types\_en.ttl | \<http://www.w3.org/1999/02/22-rdf-syntax-ns#type\> | X | extractResults | SPARQL |
| I | instance\_types\_sdtyped\_dbo\_en.ttl | \<http://www.w3.org/1999/02/22-rdf-syntax-ns#type\> | X | extractResults | SPARQL |
| J | instance\_types\_transitive\_en.ttl | \<http://www.w3.org/1999/02/22-rdf-syntax-ns#type\> | X | extractResults | SPARQL |
| K | transitive\_redirects\_en.ttl | \<[http://dbpedia.org/ontology/wikiPageRedirects](http://dbpedia.org/ontology/wikiPageRedirects)\> | X | extractResults | SPARQL |
| L | redirects\_en.ttl | \<[http://dbpedia.org/ontology/wikiPageRedirects](http://dbpedia.org/ontology/wikiPageRedirects)\> | X | extractResults | SPARQL |
| M | disambiguations\_en.ttl | [http://dbpedia.org/ontology/wikiPageDisambiguates](http://dbpedia.org/ontology/wikiPageDisambiguates) | X | extractResults | SPARQL |
| N | interlanguage\_links\_en.ttl | [http://www.w3.org/2002/07/owl#sameAs](http://www.w3.org/2002/07/owl#sameAs) | X | getDescription | SPARQL |
| O | yago\_taxonomy.ttl | \<[http://www.w3.org/2000/01/rdf-schema#subClassOf](http://www.w3.org/2000/01/rdf-schema#subClassOf)\> | X | readYAGOOntology | READ |
| P | yago\_type\_links.ttl | http://www.w3.org/2002/07/owl#equivalentClass | X | readYAGOOntology | READ |
| P2 | dbpedia\_2016-10.nt | \<http://www.w3.org/2000/01/rdf-schema#label\>\<http://www.w3.org/2000/01/rdf-schema#comment\>\<http://www.w3.org/2000/01/rdf-schema#subClassOf\>\<http://www.w3.org/ns/prov#wasDerivedFrom\> |
 | readDbPediaOntology | READ |
| Q | umbel\_reference\_concepts\_v100.n3 (OLD) | rdf: \<http://www.w3.org/1999/02/22-rdf-syntax-ns#\> .rdfs: \<http://www.w3.org/2000/01/rdf-schema#\> .skos: \<http://www.w3.org/2004/02/skos/core#\> .rdf:type, rdfs:subClassOf, skos:prefLabel |
 | readUMBELNS (OLD) | READ(OLD) |
| R | umbel downloads\*.n3 (NEW) | owl: \<http://www.w3.org/2002/07/owl#\> . rdfs: \<http://www.w3.org/2000/01/rdf-schema#\> .umbel: \<http://umbel.org/umbel#\> .owl:sameAsowl:equivalentClassrdfs:subClassOfumbel:isRelatedToa \<http://www.w3.org/1999/02/22-rdf-syntax-ns#type\> |
 | readUMBELNS (NEW) | READ |
| S | J:\caches\freebase-rdf-2014-02-09-00-00 \freebase-rdf-2014-02-09-00-00 (OLD) | \<http://rdf.freebase.com/ns/type.object.type\>\<http://rdf.freebase.com/ns/type.object.name\>\<http://rdf.freebase.com/ns/common.topic.description\>\<http://rdf.freebase.com/key/key.en\>\<http://rdf.freebase.com/ns/common.topic.article\>\<http://rdf.freebase.com/ns/common.document.text\>\<http://rdf.freebase.com/ns/common.topic.alias\>\<http://rdf.freebase.com/key/key.wikipedia.en\>\<http://rdf.freebase.com/ns/people.person.profession\> |
 | reduceLocalFreebase(OLD) | READ(OLD) |
| U | http://acronyms.thefreedictionary.com | acronyms |
 | getAcronyms | WEB |

Caches and other files:

|
 | FILE or SOURCE | POPULATING PROCEDURE | POPULATED FROM |
| --- | --- | --- | --- |
| A | source\\lists\\umbel\_superClassCache | readUMBELNS | Source R |
| B | source\\lists\\dbPediaOntology.txt | readDbPediaOntology (OLD) | Unknown |
| C | source\lists\dbpedia\_2016-10.nt | readDbPediaOntology (NEW) | wiki.dbpedia.org/develop/datasets/downloads-2016-10#dbpedia-ontology |
| D | J:\\caches\\dbPediaCache\\_rdfTypes | fillOntologyListreadYAGOOntologyreadDbPediaOntologyreadUMBELSuperClasses | YAGO - O,PUMBEL - R |
| E | freebaseProperties table in mysql | reduceLocalFreebase | S |

Structure members:

1. wstring label – derives from [http://www.w3.org/2002/07/owl#equivalentClass](http://www.w3.org/2002/07/owl#equivalentClass) (yago\_type\_links.ttl) – example 'carousel'
2. wstring infoPage - wikipedia page – filled by getDescription – not used
3. wstring abstractDescription long\_abstracts\_en.ttl: [http://dbpedia.org/ontology/abstract](http://dbpedia.org/ontology/abstract)– not used
4. wstring commentDescription - short\_abstracts\_en.ttl: \<http://www.w3.org/2000/01/rdf-schema#comment\>– not used
5. int numLine – which line – not used
6. int ontologyHierarchicalRank – populated by findCategoryRank and fillRanks, used in setPreferred (set rdfType preferred for topHierarchyClassIndexes, rdfType preferredUnknownClass for all rdfTypes.
7. int ontologyType - dbPedia\_Ontology\_Type 1, YAGO\_Ontology\_Type 2, UMBEL\_Ontology\_Type 3, OpenGIS\_Ontology\_Type 4
8. bool descriptionFilled
9. vector \<wstring\> superClasses

## Syntactic Relations

| Constant | Description/Example |
| --- | --- |
| SubjectWordWithVerb | The plumber worked. |
| SubjectWordWithNotVerb | The plumber didnot work. |
| VerbWithDirectWord | The boythrew the ball. |
| VerbWithIndirectWord | The boythrew his father the ball. |
| DirectWordWithIndirectWord | The boythrew his father the ball. |
| VerbWithInfinitive | The boy wanted to help his father. |
| WordWithInfinitive, AWordWithInfinitive | Not used |
| AVerbWithInfinitive | Not used |
| VerbWithPrep, AVerbWithPrep | Hewentto the store. |
| WordWithPrep, AWordWithPrep | Hewentto the storefromhishouse. |
| PwordWithPrep | Hewentto the store. |
| MasterVerbWithVerb | Not used |
| AdjectiveWithWord | The bedraggled man |
| AdjectiveIRNotWord | Not used - a negated identity relation He is NOT fast |
| AdverbWithVerb | He quickly ran. |
| RelativeWithWord | when relative clause is attached to word |
| VerbWithTimePrep | when a preposition associated with a time is attached to a verb |
| VerbWithParticle | track when a participle is really a particle |
| VerbWithNext1MainVerbSameSubject | verbs are same tense and close to one another |
| VerbWithNext1MainVerb | verbs are same tense and close to one another |
| WordWithPrepObjectWord | the object of a preposition is linked to object |

Fields per position

relPrep

example:

relObject

relSubject

relVerb

relNextObject

relInternalVerb

relInternalObject

nextCompoundPartObject

previousCompoundPartObject

## Disk Space Usage:

J:

webSearchCache - 310GB in 1.3M files

M:

texts - 1.12TB in 113K files

dbPediaCache - reduced to 63G in 1.3M files

acronym cache - 20GB in 444K files

dictionaryDotCom - 3.2GB in 26K files

gutenbergCatalog - 900MB

Webster - 635MB in 1M files

wordNetCache - 7MB

wikipediaCache - 3.49GB in 1.3M files

"J:\Program Files\MySQL\MySQL Server 8.0\bin\mysqld.exe" --defaults-file="F:\ProgramData\MySQL\MySQL Server 8.0\my.ini" MySQL80(2)

## References

Clark, K. (n.d.). Neural Coreference Resolution. 8.

Wikipedia. (2019, March 26th). _Precision and Recall_. Retrieved from Wikipedia: https://en.wikipedia.org/wiki/Precision\_and\_recall

Institution: Cognitive Computation Group at U of Penn

Package/Project Name: Illinois Corefence Package

Coreference Website: [https://cogcomp.org/page/software\_view/Coref](https://cogcomp.org/page/software_view/Coref)

Principals: Eric Bengtson and Dan Roth, Mark Sammons, Rajhans Samdani, Alla Rozovskaya, Kai-Wei Chang

- [Understanding the Value of Features for Coreference Resolution](http://cogcomp.org/papers/BengtsonRo08.pdf)

Eric Bengtson and Dan Roth, _EMNLP _(2008)

- [A Joint Framework for Coreference Resolution and Mention Head Detection ](https://cogcomp.org/page/publication_view/771)
 Haoruo Peng, Kai-Wei Chang Dan Roth, _CoNLLUniversity of Illinois at Urbana-Champaign - 2015_
- [A Constrained Latent Variable Model for Coreference Resolution ](https://cogcomp.org/page/publication_view/732)
 Kai-Wei Chang, Rajhans Samdani and Dan Roth, _EMNLP - 2013_
- [Illinois-Coref: The UI System in the CoNLL-2012 Shared Task ](https://cogcomp.org/page/publication_view/701)
 Kai-Wei Chang, Rajhans Samdani, Alla Rozovskaya, Mark Sammons and Dan Roth, _CoNLL - 2012_
- [Inference Protocols for Coreference Resolution ](https://cogcomp.org/page/publication_view/669)
 Kai-Wei Chang and Rajhans Samdani and Alla Rozovskaya and Nick Rizzolo and Mark Sammons and Dan Roth, _CoNLL - 2011_
- [Understanding the Value of Features for Coreference Resolution ](https://cogcomp.org/page/publication_view/188)
 Eric Bengtson and Dan Roth, _EMNLP – 2008_

Institution: AI2: Allen Institute for Artificial Intelligence

Package/Project Name: AllenNLP: A Deep Semantic Natural Language Processing Platform

Website: [https://allennlp.org/](https://allennlp.org/)

Principals: Matt Gardner and Joel Grus and Mark Neumann and Oyvind Tafjord and Pradeep Dasigi and Nelson F. Liu and Matthew Peters and Michael Schmitz and Luke S. Zettlemoyer

arXiv:1803.07640

Eraldo Rezende Fernandes, C´ıcero Nogueira Dos Santos, and Ruy Luiz Milidi´u.

Latent structure perceptron with feature induction for unrestricted coreference resolution. In Proceedings of the Joint Conference on Empirical Methods in Natural Language Processing and Conference on Computational Natural Language Learning - Shared Task, pages 41–48, 2012.

Fernandes, E.R., Santos, C.N., & Milidiú, R.L. (2014). Latent Trees for Coreference Resolution. _Computational Linguistics, 40_, 801-835.

Moschitti, Alessandro and Iryna Haponchyk. "A Practical Perspective on Latent Structured Prediction for Coreference Resolution." _EACL_ (2017).

Lappin and Leass – An algorithm for Pronomial Anaphora Resolution.

Kevin Clark and Chris Manning.

Entity-centric coreference resolution with model stacking. In Association of Computational Linguistics (ACL), 2015.

Greg Durrett and Dan Klein.

A joint model for entity analysis: Coreference, typing, and linking. Transactions of the Association for Computational Linguistics (TACL), 2:477–490, 2014.

Chao Ma, Janardhan Rao Doppa, J Walker Orr, Prashanth Mannem, Xiaoli Fern, Tom Dietterich, and Prasad Tadepalli.

Prune-and-score: Learning for greedy coreference resolution. In Empirical Methods in Natural Language Processing (EMNLP), 2014.

Anders Bj¨orkelund and Jonas Kuhn.

Learning structured perceptrons for coreference resolution with latent antecedents and non-local features. In Association of Computational Linguistics (ACL), 2014.

Clark, K. (n.d.). Neural Coreference Resolution. 8.

Wikipedia. (2019, March 26th). _Precision and Recall_. Retrieved from Wikipedia: https://en.wikipedia.org/wiki/Precision\_and\_recall

Top of Form

[Building Natural Language Generation Systems (Studies in Natural Language Processing) ](https://www.amazon.com/gp/product/0521620368/ref=ox_sc_act_title_1?smid=ATVPDKIKX0DER&psc=1)by Ehud Reiter

[Open-Domain Question Answering from Large Text Collections (Studies in Computational Linguistics) by Marius Pasca (2003-04-01)](https://www.amazon.com/gp/product/B01FJ1RYOE/ref=ox_sc_act_title_2?smid=A2KI6L13PQGY9Y&psc=1)

[Advances in Open Domain Question Answering (Text, Speech and Language Technology) ](https://www.amazon.com/gp/product/1402047452/ref=ox_sc_act_title_3?smid=A1KIF2Y9A1PQYE&psc=1)by Tomek Strzalkowski

[Everyday Conversation ](https://www.amazon.com/gp/product/157766079X/ref=ox_sc_act_title_4?smid=A1I1V7QUWSMYGA&psc=1)by Robert E. Nofsinger

[Modelling Spatial Knowledge on a Linguistic Basis: Theory - Prototype - Integration (Lecture Notes in Computer Science) ](https://www.amazon.com/gp/product/354053718X/ref=ox_sc_act_title_5?smid=A3DO1GYIAY24V1&psc=1)by Ewald Lang

[The Oxford Handbook of Computational Linguistics (Oxford Handbooks) (March 10, 2005)](https://www.amazon.com/gp/product/B015X4M8ZC/ref=ox_sc_act_title_6?smid=A3NS59O9EWBRAO&psc=1)

[The Emotion Machine: Commonsense Thinking, Artificial Intelligence, and the Future of the Human Mind ](https://www.amazon.com/gp/product/0743276639/ref=ox_sc_act_title_7?smid=A2M70HDXL9YHS&psc=1)by Marvin Minsky

[Sequence Organization in Interaction: A Primer in Conversation Analysis ](https://www.amazon.com/gp/product/0521532795/ref=ox_sc_act_title_8?smid=A3TJVJMBQL014A&psc=1)by Emanuel A. Schegloff

[Cognitive Linguistics: Current Applications and Future Perspectives (Mouton Reader) ](https://www.amazon.com/gp/product/3110189518/ref=ox_sc_act_title_9?smid=ATVPDKIKX0DER&psc=1)by Gitte Kristiansen

[Representation and Inference for Natural Language: A First Course in Computational Semantics (Studies in Computational Linguistics) ](https://www.amazon.com/gp/product/1575864967/ref=ox_sc_act_title_10?smid=A3KHMEC8P675KP&psc=1)by Patrick Blackburn

[Narrative Progression in the Short Story: A corpus stylistic approach (Linguistic Approaches to Literature) ](https://www.amazon.com/gp/product/9027233438/ref=ox_sc_act_title_11?smid=AX7I551XUYEIT&psc=1)by Michael Toolan

[Foundations of Statistical Natural Language Processing ](https://www.amazon.com/gp/product/0262133601/ref=ox_sc_act_title_12?smid=ATVPDKIKX0DER&psc=1)by Christopher D. Manning

[Statistically-Driven Computer Grammars Of English: The IBM/Lancaster Approach (Language and Computers) ](https://www.amazon.com/gp/product/9051834780/ref=ox_sc_act_title_13?smid=AP3VA1GJZM3EQ&psc=1)by Ezra Black

[The Imagined Moment: Time, Narrative, and Computation (Frontiers of Narrative) ](https://www.amazon.com/gp/product/0803229771/ref=ox_sc_act_title_14?smid=ATVPDKIKX0DER&psc=1)by Inderjeet Mani

[Tense and Aspect: From Semantics to Morphosyntax (Oxford Studies in Comparative Syntax) ](https://www.amazon.com/gp/product/0195091930/ref=ox_sc_act_title_15?smid=A3TJVJMBQL014A&psc=1)by Alessandra Giorgi

[Tense (Cambridge Textbooks in Linguistics) ](https://www.amazon.com/gp/product/0521281385/ref=ox_sc_act_title_16?smid=A3TJVJMBQL014A&psc=1)by Comrie

[Statistical Language Learning (Language, Speech, and Communication) ](https://www.amazon.com/gp/product/0262032163/ref=ox_sc_act_title_17?smid=ALPP9J6013XGH&psc=1)by Eugene Charniak

[Automatic Ambiguity Resolution in Natural Language Processing: An Empirical Approach (Lecture Notes in Computer Science) ](https://www.amazon.com/gp/product/3540620044/ref=ox_sc_act_title_18?smid=ATVPDKIKX0DER&psc=1)by Alexander Franz

[[(Computational Linguistics and Formal Semantics)] [Author: Michael Rosner] published on (December, 2010)](https://www.amazon.com/gp/product/B00Y30RQFA/ref=ox_sc_act_title_19?smid=A3N68MF9MUNXV9&psc=1)

[English Verb Classes and Alternations: A Preliminary Investigation ](https://www.amazon.com/gp/product/0226475336/ref=ox_sc_act_title_20?smid=A1KIF2Y9A1PQYE&psc=1)by Beth Levin

[Interpreting Anaphors in Natural Language Texts (Ellis Horwood Books in Computing Science, Series in Artificial intelligence) ](https://www.amazon.com/gp/product/0134685210/ref=ox_sc_act_title_21?smid=A1SBTCJFTOKRXQ&psc=1)by David Carter

[Mind Design: Philosophy, Psychology, and Artificial Intelligence (1981-07-13)](https://www.amazon.com/gp/product/B01FKWP02K/ref=ox_sc_act_title_22?smid=A1UXX1AR3OGHAA&psc=1)

[Naive Semantics for Natural Language Understanding (The International Series in Engineering and Computer Science) ](https://www.amazon.com/gp/product/0898382874/ref=ox_sc_act_title_23?smid=A1KIF2Y9A1PQYE&psc=1)by Kathleen Dahlgren

[Anaphora in Natural Language Understanding: A Survey (Lecture Notes in Computer Science) by G. Hirst (1981-09-10)](https://www.amazon.com/gp/product/B019NS0UY0/ref=ox_sc_act_title_24?smid=A1499G17ZVTYFI&psc=1)

[Natural Language Processing Using Very Large Corpora (TEXT, SPEECH AND LANGUAGE TECHNOLOGY Volume 11) ](https://www.amazon.com/gp/product/0792360559/ref=ox_sc_act_title_25?smid=A2NGELW3TL9Y11&psc=1)by Susan Armstrong

[Conceptual Structures: Information Processing in Mind and Machine (SYSTEMS PROGRAMMING SERIES) ](https://www.amazon.com/gp/product/0201144727/ref=ox_sc_act_title_26?smid=A3M5XKDZ9HMJI2&psc=1)by John F. Sowa

[Language Processes ](https://www.amazon.com/gp/product/0030635896/ref=ox_sc_act_title_27?smid=AG6GM3V5CRU18&psc=1)by Vivien C. Tartter

[Representing Time in Natural Language: The Dynamic Interpretation of Tense and Aspect ](https://www.amazon.com/gp/product/0262700662/ref=ox_sc_act_title_28?smid=A3657B0KL9ZR59&psc=1)by Alice G.B. ter Meulen

[Anaphora Resolution (Studies in Language and Linguistics) ](https://www.amazon.com/gp/product/0582325056/ref=ox_sc_act_title_29?smid=A3TJVJMBQL014A&psc=1)by Ruslan Mitkov

[From Discourse to Logic: Introduction to Modeltheoretic Semantics of Natural Language, Formal Logic and Discourse Representation Theory (Studies in Linguistics and Philosophy) ](https://www.amazon.com/gp/product/0792310284/ref=ox_sc_act_title_30?smid=ATVPDKIKX0DER&psc=1)by Hans Kamp

[The Theory and Practice of Discourse Parsing and Summarization (A Bradford Book) ](https://www.amazon.com/gp/product/0262133725/ref=ox_sc_act_title_31?smid=ATVPDKIKX0DER&psc=1)by Daniel Marcu

[Computational Linguistics: An Introduction (Studies in Natural Language Processing) ](https://www.amazon.com/gp/product/0521310385/ref=ox_sc_act_title_32?smid=A3TJVJMBQL014A&psc=1)by Ralph Grishman

[Language As a Cognitive Process: Syntax ](https://www.amazon.com/gp/product/0201085712/ref=ox_sc_act_title_33?smid=A27B5DY16DWKU4&psc=1)by Terry Winograd

[Aspects of the Theory of Syntax ](https://www.amazon.com/gp/product/0262530074/ref=ox_sc_act_title_34?smid=A29G165BTNNM2Z&psc=1)by Noam Chomsky

[Syntactic Structures Janua Linguarum NR4 ](https://www.amazon.com/gp/product/B000PV6044/ref=ox_sc_act_title_35?smid=A18OZMH8UQINVM&psc=1)by Noam Chomsky

[Ontology Learning and Population from Text: Algorithms, Evaluation and Applications ](https://www.amazon.com/gp/product/1441940324/ref=ox_sc_act_title_36?smid=A1KIF2Y9A1PQYE&psc=1)by Philipp Cimiano

[Corpus Processing for Lexical Acquisition (Language, Speech, and Communication) ](https://www.amazon.com/gp/product/026202392X/ref=ox_sc_saved_title_1?smid=A3K7ZC3ZEPJY6T&psc=1)by Branimir Boguraev

[The Language of Time: A Reader ](https://www.amazon.com/gp/product/0199268541/ref=ox_sc_saved_title_2?smid=ATVPDKIKX0DER&psc=1)by Inderjeet Mani

[Cognitive Development and Acquisition of Language ](https://www.amazon.com/gp/product/0125058500/ref=ox_sc_saved_title_3?smid=A18OZMH8UQINVM&psc=1)by Timothy E. Moore

[Artificial Intelligence: A Modern Approach](https://www.amazon.com/dp/B008NYIYZS/?coliid=I1HUNY950KGBXG&colid=VNKPO3BWAY4B&psc=0&ref_=lv_ov_lig_dp_it)

[Comprehensive Grammar of the English Language](https://www.amazon.com/dp/0582517346/?coliid=I3P5SXMNEOJPHD&colid=VNKPO3BWAY4B&psc=0&ref_=lv_ov_lig_dp_it) – Randolph Quirk, Sidney Greenbaum, Geoffry Leech, Jan Svartvik

[Longman Grammar of Spoken and Written English](https://www.amazon.com/dp/0582237254/?coliid=I3C04WJ2YYJZMA&colid=VNKPO3BWAY4B&psc=0&ref_=lv_ov_lig_dp_it) by Douglas Biber, Stig Johansson

## Competitive Section

Boost AI – boost.ai

Woebot

Emotion labs

Joyable

Youper

X2AI

WellBe

Sumondo

iBreve

[https://www.cbinsights.com/company/tnh-health](https://www.cbinsights.com/company/tnh-health)

Mitsuko

Replika

Characteristics

In app text based games like Maze

Points awarded based on responding

Instagram with pictures

Message of the day

Suggested responses in bubbles so people don't have to type
