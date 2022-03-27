#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "word.hpp"
#include "result.hpp"
#include "dictionary.hpp"
#include "solver.hpp"

using std::string;
using std::vector;
using std::set;
using std::pair;
using std::map;
using std::cout;
using std::endl;
typedef Dictionary::WordIndex WordIndex;
typedef boost::posix_time::ptime ptime;
using namespace Db;

namespace Solver {
    const Word no_best_guess("BBBBB");
    const Word no_worst_answer("WWWWW");
    
    SolveResult::SolveResult() : best_score(99999), best_guess(no_best_guess), worst_answer(no_worst_answer), perf_calls(1), perf_microseconds(0) {};

    vector<WordIndex> valid_list(const CMask& m, const vector<WordIndex>& dict) {
        vector<WordIndex> r;
        r.reserve(dict.size());
        for (WordIndex w : dict) {
            if (m.check(*w)) r.push_back(w);
        }
        return r;
    }

    // returns count of valid answers, only outputting the first two
    int valid_count(const CMask& m, const vector<WordIndex>& dict, Word::Compact& out1, Word::Compact& out2) {
        int c = 0;
        for (WordIndex w : dict) {
            if (m.check(*w)) {
                if (c==0) {
                    out1 = *w;
                } else if (c==1) {
                    out2 = *w;
                }
                c++;                
            }
        }
        return c;
    }
    
    ptime now() {
        return boost::posix_time::microsec_clock::local_time();
    }
    
    const bool adversarial = true; // Code is kinda broken for average case right now.
    const unsigned int max_num_results_any_guess = 150;
    
    vector<pair<int, WordIndex>> sort_by_heuristic(const vector<WordIndex>& answers,
                                                    const vector<WordIndex>& guesses,
                                                    const CMask& m) {

         vector<pair<int, WordIndex>> scores;
         for (WordIndex g : guesses) {
             int still_valid_count = 0;

             map<CMask, int> cache;
             for (WordIndex a : answers) {
                 CMask nr(*a, *g);
                 if (!cache.count(nr)) {
                     CMask nm(m);
                     nm.apply(nr);
                     int sv = 0;
                     for (WordIndex a2 : answers) {
                         if (nm.check(*a2)) sv++;
                     }
                     cache.insert({nr, sv});
                 }
                 still_valid_count += cache.at(nr);
             }
             scores.push_back({still_valid_count, g});
         }
         std::sort(scores.begin(), scores.end());
         return scores;
    }
    
    SolveResult solve_p(Db_intf* db,
                        const vector<WordIndex>& prev_valid_answers,
                        const vector<WordIndex>& prev_valid_guesses,
                        const CMask& m,
                        float score_cutoff,
                        bool debug_extra_info_top_level,
                        bool track_time,
                        ptime timeout) {

        SolveResult rv;
        if (db && db->query(m, Job::no_guess, Objective::adversarial, rv)) return rv;

        ptime start;
        if (track_time || timeout != boost::posix_time::pos_infin) {
            start = now();
            if (start > timeout) throw std::runtime_error("timeout");
        }
        
        if (m.has_at_most_one_letter_undetermined()) {
            int num_valid = valid_count(m, prev_valid_answers, rv.best_guess, rv.worst_answer);
            if (num_valid == 1) {
                rv.worst_answer = rv.best_guess;
            }
            if (adversarial) {
                rv.best_score = num_valid;
            } else {
                rv.best_score = (1.0 + num_valid) / 2.0f;
            }
            if (track_time) rv.perf_microseconds = (now() - start).total_microseconds();

            if (debug_extra_info_top_level) {
                cout <<  "At most one letter, determined, no choice but to go through these " << num_valid << endl;
                for (WordIndex answer : valid_list(m, prev_valid_answers)) {
                    cout << " " << *answer << endl;
                }
            }
            return rv;                
        }
        
        vector<WordIndex> valid_answers = valid_list(m, prev_valid_answers);
        vector<WordIndex> valid_guesses = valid_list(m, prev_valid_guesses);
        vector<WordIndex> guesses_to_check;
        const vector<WordIndex>* guess_to_check_ptr = nullptr; // will point to one of the above
    
        if (debug_extra_info_top_level) {
            vector<pair<int, WordIndex>> scores = sort_by_heuristic(valid_answers, valid_guesses, m);
            std::reverse(scores.begin(), scores.end());
            guesses_to_check.reserve(valid_guesses.size());
            for (unsigned int i = 0 ; i < valid_guesses.size() ; i++) {
                guesses_to_check.push_back(scores[i].second);
            }
            if (debug_extra_info_top_level) {
                cout << "Sorted " << valid_guesses.size() << " by hueristic: " << guesses_to_check.size() << endl;
            }
            guess_to_check_ptr = &guesses_to_check;
        } else {
            guess_to_check_ptr = &valid_guesses;
        }

        if (debug_extra_info_top_level || valid_answers.empty() || valid_guesses.empty()) {
            cout <<  "num_valid_answers: " << valid_answers.size() << " num_valid_guesses: " << valid_guesses.size() << endl;
        }
    
        if (valid_answers.empty() || valid_guesses.empty()) {
            throw std::runtime_error("Got no answers or guesses?");
        }

        if (valid_answers.size() == 1) {
            rv.best_score = 1;
            rv.best_guess = *valid_answers[0];
            rv.worst_answer = *valid_answers[0];
            if (track_time) rv.perf_microseconds = (now() - start).total_microseconds();
            return rv;
        } else if (valid_answers.size() == 2) {
            if (adversarial) {
                rv.best_score = 2;
            } else {
                rv.best_score = 1.5;
            }
            rv.best_guess = *valid_answers[0];
            rv.worst_answer = *valid_answers[1];
            if (track_time) rv.perf_microseconds = (now() - start).total_microseconds();
            return rv;
        }

        float best_possible_score ;
        if (adversarial) {
            if (valid_answers.size() > max_num_results_any_guess) {
                best_possible_score = 3;
            } else {
                best_possible_score = 2;
            }
        } else {
            best_possible_score = 1.0f + (valid_answers.size () - 1.0) / valid_answers.size();
        }
        if (best_possible_score >= score_cutoff) {
            //std::cout << "cutoff " << m << " " << best_possible_score  << " >= " << score_cutoff << std::endl;
            // CR fix math for non-adversarial
            rv.best_score = score_cutoff;
            rv.best_guess = *valid_answers[0];
            rv.worst_answer = *valid_answers[1]; // this isn't necessarily correct in the adversarial-3 case
            if (track_time) rv.perf_microseconds = (now() - start).total_microseconds();
            return rv;
        }
    
        if (debug_extra_info_top_level && valid_answers.size() < 20) {
            for (WordIndex w : valid_answers) {
                cout << " Valid answer: " << *w << endl;
            }
        }

        double perf_calls = 1;

        map<WordIndex, float> score_by_guess;
        int next_answer_slot_to_swap_into = 0;
        for (unsigned int guess_index = 0; guess_index < guess_to_check_ptr->size(); guess_index++) {       
            WordIndex guess = (*guess_to_check_ptr)[guess_index];
            if (debug_extra_info_top_level) {
                cout << now() << " On guess #" << guess_index << "/" << valid_guesses.size() << ": " << *guess << "..." << std::flush;
            }

            float score_to_use;
            WordIndex worst_answer = guess; // overwritten later in adversarial, left as this garbage otherwise
            // Normally we don't care unless this guess can improve on the best (rv.best_score), but if we are
            // listing all the best guesses we actually care whether this guess matches the best or is worse.
            float new_cutoff = std::min(score_cutoff, rv.best_score + (debug_extra_info_top_level ? 0.1f : 0));

            if (adversarial) {
                int worst_answer_index;
                SolveResult this_guess_worst_case =
                    solve_c(db,
                            valid_answers,
                            valid_guesses,
                            CMask(m),
                            guess,
                            new_cutoff,
                            false,
                            false,
                            &worst_answer_index,
			    timeout);

		perf_calls += this_guess_worst_case.perf_calls; 

                worst_answer = valid_answers[worst_answer_index];
                if (worst_answer_index > next_answer_slot_to_swap_into) {
                    // hack to sort worst cases up to exit earlier
                    std::swap(valid_answers[worst_answer_index], valid_answers[next_answer_slot_to_swap_into]);
                    next_answer_slot_to_swap_into++;
                }
                score_to_use = this_guess_worst_case.best_score;
            } else {
                double sum_answer_s = 0;            
                map<CMask, float> possible_results;
                for (unsigned int answer_index = 0 ; answer_index < valid_answers.size() ; answer_index++) {
                    WordIndex answer = valid_answers[answer_index];
                    CMask nr(*answer, *guess);
                    if (!possible_results.count(nr)) {
		        SolveResult sr = solve_p(nullptr, valid_answers, valid_guesses, CMask(m).apply(nr), new_cutoff - 1, false, false, timeout);
		        perf_calls += sr.perf_calls; 
		        // CR fix math for non-adversarial
		        possible_results[nr] = sr.best_score + 1;
                    }
                    float s = possible_results.at(nr);
                    sum_answer_s += s;
                }
                score_to_use = sum_answer_s / valid_answers.size();
            }
            if (debug_extra_info_top_level) {
                score_by_guess.insert({guess, score_to_use});
            }

            // we have a score
            if (score_to_use < rv.best_score) {
                if (debug_extra_info_top_level) {
                    cout << " took " << score_to_use << " steps (worst answer " << *worst_answer << "), is new best, prev: " << rv.best_guess << " with " << rv.best_score << endl;
                }
                rv.best_score = score_to_use;
                rv.worst_answer = *worst_answer;
                rv.best_guess = *guess;
                if (adversarial && score_to_use == 2 && !debug_extra_info_top_level) break;
            } else if (score_to_use == rv.best_score) {
                if (debug_extra_info_top_level) {
                    cout << " took " << score_to_use << " steps (worst answer " << *worst_answer << "), tied with prev: " << rv.best_guess << " with " << rv.best_score << endl;
                }
            } else {
                if (debug_extra_info_top_level) {
                    cout << " took " << score_to_use << " steps (worst answer " << *worst_answer << "), loses to prev: " << rv.best_guess << " with " << rv.best_score << endl;
                }
            }
        }
        if (track_time) rv.perf_microseconds = (now() - start).total_microseconds();
        rv.perf_calls = perf_calls;

        if (debug_extra_info_top_level) {
            for (auto const& guess_and_score : score_by_guess) {
                WordIndex guess = guess_and_score.first;
                float score = guess_and_score.second;
                if (score == rv.best_score) {
                    cout << " Equally good guesses: " << *guess << " with score " << score << endl;
                }
            }        
        }
    
        return rv;
    }

     // always adversarial
    SolveResult solve_c(Db_intf* db,
                        const vector<WordIndex>& valid_answers,
                        const vector<WordIndex>& prev_valid_guesses,
                        const CMask& m,
                        WordIndex guess,
                        float score_cutoff,
                        bool debug_extra_info_top_level,
			bool track_time,
                        int* out_worst_answer_index,
                        ptime timeout) {
        if (debug_extra_info_top_level || valid_answers.empty() || prev_valid_guesses.empty()) {
            cout <<  "num_valid_answers: " << valid_answers.size() << " num_valid_guesses: " << prev_valid_guesses.size() << endl;
        }

        ptime start;
        if (track_time || timeout != boost::posix_time::pos_infin) {
            start = now();
            if (start > timeout) throw std::runtime_error("timeout");
        }
	
         SolveResult rv;
         if (db->query(m, *guess, Objective::adversarial, rv)) {
             if (out_worst_answer_index) {
                 for (unsigned int answer_index = 0 ; answer_index < valid_answers.size() ; answer_index++) {
                     if (Word::Compact(*valid_answers[answer_index]) == rv.worst_answer) {
                         *out_worst_answer_index = answer_index;
                         return rv;
                     }
                 }
                 throw std::runtime_error("Maybe db is corrupted? got hit no answer isn't valid");
             } else {
                 return rv;
             }

         }
         rv.best_score = 0;
         rv.best_guess = *guess;
        
         map<CMask, float> possible_results;
         for (unsigned int answer_index = 0 ; answer_index < valid_answers.size() ; answer_index++) {
             WordIndex answer = valid_answers[answer_index];
             CMask nr(*answer, *guess);                
             if (debug_extra_info_top_level) {
                 cout << now() << " On answer #" << answer_index << "/" << valid_answers.size() << ": " << *answer << "..." << std::flush;
             }
             if (!possible_results.count(nr)) {
                 if (answer == guess) {
                     possible_results[nr] = 1;
                 } else {
                     SolveResult sr = solve_p(db, valid_answers, prev_valid_guesses, CMask(m).apply(nr), score_cutoff - 1, false, false, timeout);
                     rv.perf_calls += sr.perf_calls; 
                     possible_results[nr] = sr.best_score + 1;
                 }
             }
             float s = possible_results.at(nr);
             if (s > rv.best_score) {
                 if (debug_extra_info_top_level) {
                     cout << " took " << s << " steps, is new best, prev: " << rv.worst_answer << " with " << rv.best_score << endl;
                 }
                 rv.best_score = s;
                 rv.worst_answer = *answer;
                 if (out_worst_answer_index) *out_worst_answer_index = answer_index;
             } else if (s == rv.best_score && debug_extra_info_top_level) {
                 if (debug_extra_info_top_level) { 
                     cout << " took " << s << " steps, tied with prev: " << rv.worst_answer << " with " << rv.best_score << endl;
                 }
             } else {
                 if (debug_extra_info_top_level) {
                     cout << " took " << s << " steps, loses to prev: " << rv.worst_answer << " with " << rv.best_score << endl;
                 }
             }
             if (s >= score_cutoff) break;
         }
         if (track_time) rv.perf_microseconds = (now() - start).total_microseconds();

         if (debug_extra_info_top_level) {
             for (WordIndex answer : valid_answers) {
                 CMask nr(*answer, *guess);
                 if (!possible_results.count(nr)) {
                     SolveResult sr = solve_p(nullptr, valid_answers, prev_valid_guesses, CMask(m).apply(nr), score_cutoff - 1, false, false, timeout);
                     possible_results[nr] = sr.best_score + 1;
                 }
                 float s = possible_results.at(nr);
                 if (s == rv.best_score) {
                     cout << " Equally good answer: " << *answer << " with score " << s << endl;
                 }
             }        
         }
    
         return rv;
     }
    
    SolveResult solve_b(Db::Db_intf* db,
                        const vector<WordIndex>& prev_valid_answers,
                        const vector<WordIndex>& prev_valid_guesses,
                        const CMask& m,
                        int num_turns,
                        float cutoff,
                        bool debug_extra_info_top_level,
                        bool track_time,
                        boost::posix_time::ptime timeout
                        ) {
        SolveResult rv;
        if (num_turns <= 0) {
            rv.best_score = 0;
            return rv;
        }
        if (db && num_turns <= 5 && db->query(m, Job::no_guess, static_cast<Objective>(num_turns), rv)) {
            return rv;
        }

        ptime start;
        if (track_time || timeout != boost::posix_time::pos_infin) {
            start = now();
            if (start > timeout) throw std::runtime_error("timeout");
        }

        if (num_turns == 1 || m.has_at_most_one_letter_undetermined()) {
            int num_valid = valid_count(m, prev_valid_answers, rv.best_guess, rv.best_guess);
            if (!num_valid) {
                throw std::runtime_error("Got num_valid = 0?");
            }
            
            if (num_turns >= num_valid) {
                rv.best_score = 1;
            } else {
                rv.best_score = (static_cast<float>(num_turns)) / num_valid;
            }
            if (track_time) rv.perf_microseconds = (now() - start).total_microseconds();
            
            if (debug_extra_info_top_level) {
                cout <<  "One turn left or at most one letter determined, no choice but to go through " << num_valid << " answers" << endl;
                if (num_valid < 20) {
                    for (WordIndex answer : valid_list(m, prev_valid_answers)) {
                        cout << " " << *answer << endl;
                    }
                }
            }
            return rv;                
        }

        vector<WordIndex> valid_answers = valid_list(m, prev_valid_answers);       
        vector<WordIndex> valid_guesses = valid_list(m, prev_valid_guesses);

        if (num_turns > 2) {
            vector<pair<int, WordIndex>> sorted_guesses = sort_by_heuristic(valid_answers, valid_guesses, m);
            for (size_t i = 0; i < valid_guesses.size(); i++) {
                valid_guesses[i] = sorted_guesses[i].second;
            }
        } 

        rv.best_score = 0;
        for (unsigned int guess_index = 0; guess_index < valid_guesses.size(); guess_index++) {
            WordIndex guess = valid_guesses[guess_index];

            if (debug_extra_info_top_level) {
                cout << now() << " On guess #" << guess_index << "/" << valid_guesses.size() << ": " << *guess << "..." << std::flush;
            }

            float score_this_guess;
            if (num_turns == 2) {
                int still_valid_count = 0;
                map<CMask, int> cache;
                for (WordIndex a : valid_answers) {
                    CMask nr(*a, *guess);
                    if (!cache.count(nr)) {
                        CMask nm(m);
                        nm.apply(nr);
                        int sv = 0;
                        for (WordIndex a2 : valid_answers) {
                            if (nm.check(*a2)) sv++;
                        }
                        cache.insert({nr, sv});
                    }
                    still_valid_count += cache.at(nr);
                }
                score_this_guess = (static_cast<float>(valid_answers.size())) / still_valid_count;
            } else {
                map<CMask, float> cache;
                double sum_score = 0;
                double max_possible_score = valid_answers.size();
                for (WordIndex a : valid_answers) {
                    CMask nr(*a, *guess);
                    if (!cache.count(nr)) {
                        CMask nm(m);
                        nm.apply(nr);

                        // not really clear this cutoff thing helps
                        float new_cutoff = std::max(cutoff, rv.best_score + (debug_extra_info_top_level ? 0.0001f : 0));
                        
                        SolveResult sr = solve_b(db, valid_answers, valid_guesses, nm, num_turns - 1, new_cutoff, false, false, timeout);
                        rv.perf_calls += sr.perf_calls;
                        cache.insert({nr, sr.best_score});
                    }
                    sum_score += cache.at(nr);
                    max_possible_score -= (1 - cache.at(nr));

                    if (max_possible_score < cutoff - 0.0001f && !debug_extra_info_top_level) {
                        break;
                    }
                }
                score_this_guess = sum_score / valid_answers.size();
            }

            if (score_this_guess > rv.best_score) {                    
                if (debug_extra_info_top_level) {
                    cout << " won with p=" << score_this_guess
                         << ", is new best, prev: " << rv.best_guess
                         << " with " << rv.best_score << endl;
                }

                rv.best_guess = *guess;
                rv.best_score = score_this_guess;                
            } else if (score_this_guess == rv.best_score) {
                if (debug_extra_info_top_level) {
                    cout << " won with p=" << score_this_guess
                         << ", tied with prev: " << rv.best_guess
                         << " with " << rv.best_score << endl;
                }
            } else {
                if (debug_extra_info_top_level) {
                    cout << " won with p=" << score_this_guess
                         << ", loses to prev: " << rv.best_guess
                         << " with " << rv.best_score << endl;
                }
            }
        }

        if (track_time) rv.perf_microseconds = (now() - start).total_microseconds();        
        return rv;
    }
}
