#pragma once
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "word.hpp"
#include "dictionary.hpp"
#include "solveresult.hpp"
#include "db.hpp"

namespace Solver {
    // Solve for the players point of view, returns the best guess.
    // The score returned *includes* guess that it suggests, so is at least 1.
    SolveResult solve_p
    (Db::Db_intf* db, // could be nullptr
     const std::vector<Dictionary::WordIndex>& prev_valid_answers,
     const std::vector<Dictionary::WordIndex>& prev_valid_guesses,
     const CMask& m,
     // we are solving for mask m.
     // prev_valid_answers, prev_valid_guesses might have already been filtered
     // from previous guesses. But it's not necessary, the first thing we do is
     // filter from based on m                      
     float score_cutoff,
     //  pretend all scores >= score_cutoff are equally bad and cut off the
     // computation                      
     bool debug_extra_info_top_level,
     // outputs extra debug info, but only for the top level of the tree
     bool track_time,
     // if false we put garbage into rv.perf_microseconds
     boost::posix_time::ptime timeout = boost::posix_time::pos_infin
     );


    // Solve for the adversary's point of view, returns the answer that delays the game the longest.
    // The score returned *includes* guess we're about to make.
    SolveResult solve_c
    (Db::Db_intf* db, // could be nullptr
     const std::vector<Dictionary::WordIndex>& valid_answers,
     const std::vector<Dictionary::WordIndex>& prev_valid_guesses,
     const CMask& m,
     Dictionary::WordIndex guess,
     // we are looking for the answer that maximizes worst-case-number-of guesses
     // once the player has guesses [guess].
     //
     // Unlike the above function [valid_answers] must have already been filtered.
     // prev_valid_guesses will be filtered later on anyway.
     float score_cutoff,
     // pretend all scores >= score_cutoff are equally bad and cut off the
     // computation                      
     bool debug_extra_info_top_level,
     // outputs extra debug info, but only for the top level of the tree.
     bool track_time,
     // if false we put garbage into rv.perf_microseconds
     int* out_worst_answer_index = nullptr,
     // if non-null then outputs an index into [valid_answsers], or -1
     // if it doesn't know.
     boost::posix_time::ptime timeout = boost::posix_time::pos_infin
     );

    // Solve for the player's point of view, but try to maximize P(win in X turns | guess) against a
    // uniform distribution of answers rather than the adversarial one.
    SolveResult solve_b
    (Db::Db_intf* db,
     const std::vector<Dictionary::WordIndex>& prev_valid_answers,
     const std::vector<Dictionary::WordIndex>& prev_valid_guesses,
     const CMask& m,
     // we are solving for mask m.
     // prev_valid_answers, prev_valid_guesses might have already been filtered
     // from previous guesses. But it's not necessary, the first thing we do is
     // filter from based on m
     int num_turns,
     // 1 = try to win this turn. So all valid answers are equally likely.
     // 2 = try to win next turn. So try to narrow down as much as possible (this is the ROATE heuristic)
     // 3 = try to win the turn after the next.
     float cutoff,
     // stop computing if it looks worse than [cutoff]. at top level you want 0.
     bool debug_extra_info_top_level,
     // if false we put garbage into rv.perf_microseconds
     bool track_time,
     // outputs extra debug info, but only for the top level of the tree.
     boost::posix_time::ptime timeout = boost::posix_time::pos_infin
     );
    
    // needed to feed valid_answers into solve_c
    std::vector<Dictionary::WordIndex> valid_list(const CMask& m, const std::vector<Dictionary::WordIndex>& dict);

    // sometimes useful fast version of valid_list, only returns two arbitrary results (if the rv < 2 we leave the corresponding
    // outs alone).
    int valid_count(const CMask& m, const std::vector<Dictionary::WordIndex>& dict, Word::Compact& out1, Word::Compact& out2);

    
    std::vector<std::pair<int, Dictionary::WordIndex>> sort_by_heuristic
    (
     const std::vector<Dictionary::WordIndex>& prev_valid_answers,
     const std::vector<Dictionary::WordIndex>& prev_valid_guesses,
     const CMask& m);
}
