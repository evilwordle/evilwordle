#pragma once
#include <string>
#include <iostream>
#include "word.hpp"
#include "cmask.hpp"

// int32_t, but really we're going to pack this into the 5-bit (26-30) of the guess.
// Don't change the ints, we assume we can static cast to get the pwin number.
enum class Objective : int32_t
    { adversarial = 0,
      pwin1 = 1,
      pwin2 = 2,
      pwin3 = 3,
      pwin4 = 4,
      pwin5 = 5};

Objective objective_of_string(const std::string& str);

// A thing to be solved. If guess = no_guess then we solve for the best guess (ie.
// best move from the players point of view). If guess != no_guess we solve for
// the answer that
//     when objective = adversarial:
//          extends the game the longest (ie. from the computers point of view).
//     otherwise:
//          the probability of winning averaged across all answers
class Job {
   
public:
    Job();
    Job(const CMask& mask_, Word::Compact guess_, Objective objective_);
    bool operator<(const Job& k) const;

    void apply_mask(const CMask& m);
    void set_mask(const CMask& m);
    void set_guess(Word::Compact g);
    void set_objective(Objective o);
    
    Objective get_objective() const;
    const CMask& get_mask() const;
    Word::Compact get_guess() const;
    
    static const Word::Compact no_guess;

    static void test();
private:
    CMask mask;

    // this is really a kludge but I didn't want to re-compute the whole database
    int32_t guess_and_objective;
};

std::ostream& operator<<(std::ostream&, Objective o);
std::ostream& operator<<(std::ostream&, const Job&);
