/*
re=referring entity - the entity which is performing some action
tdo=topic descriptor object - the entity that is being acted on by the referring entity
ANS=the entity which gives the answer to the corresponding question

1. What Noun[topic descriptor object] do(did) [referring entity](you) verb (play/like to play)
   [re] verb ANS
1. What is/are Noun(ownership)[referring entity] [topic descriptor object] (favorite food)
   [re] [tdo] is/are ANS
1. Do(Did) Noun[referring entity] verb [descriptor adjective] [topic descriptor entity] (white or black coffee)
   [re] verb [ANS descriptor adjective] [topic descriptor entity]
1. How [topic adjective] is/are [referring entity]
   [referring entity] is/are [ANS - descriptor adverb] [topic adjective]
1. is/are [referring entity] [descriptor adjective] [topic descriptor entity] (a human or a computer)
   IF [descriptor adjective] [topic descriptor entity] is a choice (using OR), then
     [referring entity] is/are [ANS descriptor adjective] [ANS topic descriptor entity]
2. What [time referring entity] is it?
   time referring entity will come from internal process
2. What [time referring entity] is/will it be [translated time entity]?
   time referring calculations will come from internal process
3. Who verb [topic descriptor object] (the football)
   [ANS] verb [tdo]
3. What is [metadescriptor](the name) of [topic descriptor object]? (What is the name of my friend who likes to play football?)
    ANS=look up specific metadescriptor property of [tdo]
3. Which verb(runs) [descriptor adjective](faster), [referring entity A] or [referring entity B]?
4. Who is [descriptor adjective], [referring entity A] or [referring entity B]?
   [referring entity A or B] verb [descriptor adjective] than [referring entity A or B].
   [referring entity A or B] verb [opposite of descriptor adjective] than [referring entity A or B].
3. What would [referring entity]I verb with [topic descriptor object](a screwdriver)?
   [topic descriptor object] is used to [ANS verb]
   [referring entity] uses a [topic descriptor object] to [ANS verb]
3. What letter comes [referring adjective] letter(T)?
   Internal table routine taking an input of a letter[referring type], referring adjective and letter(T)
3. How many [referring piece entity]letters are in [referring entity]'total'?
   Internal table routine taking an input of a letter[referring type], referring adjective(in) and word('total')

5. Analyze each kind of simple question.
   create a localMultiWordRelations, to store the conversation.
   store self facts like this:
   I am 29 years old.
   determine how best to model and retrieve information for each type.
   Model question/response in narrative metalanguage.


1. Questions about facts known about self
   Name, age, height, weight, clothing, interests (sports), movies, plays, musicals, countries visited, race, parents, grandparents, children, grandchildren, sisters, brothers, cousins, nieces, nephews, occupation, workplace, coworkers, home address, home region, language, favorites (color, food, fruit, coffee) etc.
	What sports do you like to play?
	Have you watched a good film lately?
	What's your favorite food?
	What's your favorite fruit?
	Do you prefer white or black coffee?
	What is your name?
	How old are you?
	Are you a human or a computer?

2. Questions about environment:
  a. time - what time is it, what month of the year is it, what day of the month/week is it, what time of day (afternoon).  What day is tomorrow?
			What year will it be next year?
			What month of the year is it?
			What day will it be tomorrow?

3. Questions about facts
  a. facts that are dynamically entered into the environment by the audience
	    My name is Ed.  What is my name?
			The football was kicked by Fred.  Who kicked the football?
			My friend Chris likes to play football.  
			What is the name of my friend who likes to play football?
  b. news facts that are generally known
  c. questions about properties of common objects (animals, implements (silverware, screwdriver,etc))
		 Which is larger, an ant or an anteater?
		 What would I do with a screwdriver?
  d. linguistic metafacts: how many letters are in a word, words in a sentence, letters after a letter (what letter after T?)
			What letter comes after T?
			How many letters are in the word 'banana'?

4. Logic questions
  a. Questions of comparison: Steve is older than Jane.  Who is youngest, Steve or Jane?

*/