# How do I read this code?
The **Word** class represents a word. The **CMask** class represents the state of the game, meaning that it uniquely specifies what answers are allowed and which are not. All of the tricky fast optimized logic is all in CMask. The **Solver** namespace has the functions that actually do the mini-max, but that's all straightforward. Everything else just provides the command line interface with an optional layer of caching results.

---
# The rest of this description is a copy/paste from https://evilwordle.com/info

# What is Wordle?
Play it or read about the rules here: https://www.nytimes.com/games/wordle/index.html

# What is Evil Wordle?
Normal Wordle picks an answer randomly and then lets you guess against it. Evil Wordle is free to change the answer after you guess as long as it's consistent with all the previous results. It picks the result of each guess adversarially. It always chooses an answer that forces the game to go on the longest.

# How does this differ from other "evil" wordles?
For example:

- https://swag.github.io/evil-wordle/
this one doesn't seem to use the full dictionary.
- https://qntm.org/files/absurdle/absurdle.html
this one has a great detailed faq and write-up, some of the answers apply here too.
- https://areiche2.github.io/evil-wurdle/

These are all applying what I call the "roate heuristic". That means they only look one row ahead, and pick whatever word maximizes the number of remaining answer at that point. This website looks as many turns ahead as it needs to, and actually gives you the nastiest result possible rather than approximating.

Try sticking SOARE into one of the other evil wordles to see the difference quickly.

# Isn't that a lot of work to compute? Doesn't it take forever?
Indeed. I computed ~8 trillion states of the game and the server is backed by a database of ~175 million entries.

I have the core operation of checking if a board state admits a given answer/guess down to 1-2ns, and can check around 1-2MM states per second per core

# Why isn't it deterministic then?
We have the full solution at hand, so if there are multiple choices that force the game to go the longest number of turns possible we pick one randomly.

# What is the best possible score?
Go to analysis mode and click "Analyze all possible guesses". It will tell you the best possible score is 5 and there are 1220 initial guesses that work. You can always guarantee a win in 5 if you always play something this site thinks is optimal.

# What word list are you using?
The NYTimes list, at least as of March 12, 2022. These are the 2309 possible answers and 12947 possible guesses.

# Is this assuming hard mode?
This actually assumes something more strict than hard mode. The basic rule is that you can't guess a word that could be ruled out as an answer based on previous information. So in addition to having to use all greens yellows you actually have to use all the available information. In particular:

1. You aren't allowed to guess a word containing a black letter. That wouldn't be allowed to be an answer. For example the C at the end of MUSIC would not be allowed below.
2. You aren't allowed to guess a letter that was yellow in the same spot. For example the final T below violates this rule.
3. If you get a yellow *and* a black on the same letter, you aren't allowed to then guess a word with the letter twice. Same if you get a green and a black.
4. If you get two greens and a black you aren't allowed to then guess a word with the letter three times. This would also apply if there are two yellows and a black, but that would be covered by Rule 2.

# Is ROATE a good starting guess?
Some sources use the "roate heuristic" which is for the adversary to pick the answer that maximizes the number of possible remaining answers. This is equivalent to the player trying to maximize the chance of a win on the next turn for a random rather than adversarial computer. ROATE comes to the top of the list with a 1.66% chance of winning by turn 2.

Turns out this is not actually a good heuristic against adversarial wordle. If you play ROATE you won't win in less than 7 turns here. In fact none of the 200 top words according to the roate heuristic guarantee a win in 5 (#201 is ARILS), which is maybe a little surprising since almost 10% of all initial guesses guarantee a win in 5.

The roate heuristic is at least very easy to compute. You only have to evaluate about a million game states after finishing the first row (compared to around 8 trillion for the full adversarial solution).

# So what is a good starting guess?
For non-adversarial wordle, a better calculation would to be look more turns out and try to maximize the probability of a win in three or four turns rather than two. Maximizing p(win in 3 turns) puts words like TRACE and CRATE at the top of the list with 38.7% and 38.5% to win by turn three respectively (ROATE is 36.1%), but those also do very poorly in adversarial wordle (8 and 7 turns respectively). The words with the highest win-by-turn-3 probabilities that also guarantee a win in 5 are:

LEAPT (36.2%)
PALET (35.9%, not a possible answer)
PLEAT (35.6%)
CLEAN (35.4%)
SPRIT (35.4%, not a possible answer)
PROST (35.3%, not a possible answer)
PLACE (35.2%)
The words that maximize p(win in 4 turns) are CLAST (94.3%), TALCS (93.8%), and CLATS (93.7%). In the adversarial (worst) case these take 6, 7, and 6 turns. ROATE is 91.3%. The words with the highest win-by-turn-4 probabilities that also guarantee a win in 5 are:

CLAPT (93.5%, not a possible answer)
LEAPT (93.4%)
SLIPT (93.1%, not a possible answer)
CLASP (93.0%)
PALET (93.0%, not a possible answer)
SPLIT (93.0%)
SCALD (92.9%)
This all assumes perfect play. Without perfect play the guaranteed and average cases don't apply. For example in this situation (to see the answer click the link and then click "Analyze all possible guesses"):

There's exactly one word that will win adversarial wordle in 5, and it's obscure. Most of the time you will end up guessing through all of CATCH, MATCH, HATCH, BATCH, WATCH, so the winning word here covers two of the starting letter, and even then you only have one way out of the TASTY/NASTY/HASTY/FATTY/CATTY/RATTY trap. How many wordle players are going to realize that?

# Just give me a starting guess already!
If I arbitrarily pick a metric of (2*p(win in 3 turns) + 1*p(win in 4 turns)) while excluding guesses that aren't valid answers and guesses with a worst case of 7+, the best guesses are:

guess	p(win in 2 turns)	p(win in 3 turns)	p(win in 4 turns)	worst case
LEAST	1.3%	37.4%	93.4%	6
DEALT	1.1%	36.9%	93.6%	6
TRICE	1.2%	38.2%	92.5%	6
LEAPT	1.0%	36.2%	93.4%	5
TRADE	1.2%	37.1%	92.8%	6
CRANE	1.3%	37.2%	92.7%	6
CLEAT	1.0%	36.5%	93.0%	6
PLATE	1.1%	36.0%	93.2%	6
ALERT	1.4%	36.6%	92.8%	6
STALE	1.3%	36.2%	92.8%	6

# Can I ask you other questions?
email: evilwordle@gmail.com
