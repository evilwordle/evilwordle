#pragma once
#include "word.hpp"

namespace Solver {
    /* The result of Solver::solve'ing a Db::job. */

    class SolveResult {
    public:
        SolveResult();
        float best_score; // lowest score if the job is to find best guess, or highest if the job is find an adversarial answer.
        
		// we use Word::Compact here instead of WordIndex for serialization purposes
        Word::Compact best_guess;
        Word::Compact worst_answer;
		
        // performance stats
        float perf_calls;
        float perf_microseconds;

        std::string to_string() const;
        static SolveResult of_string(const std::string& r);

        bool operator<(const SolveResult& r) const;

        static void test();
    };

    std::ostream& operator<<(std::ostream& os, const SolveResult& s);
}
