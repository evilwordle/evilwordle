#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>
#include <memory>
#include "word.hpp"
#include "result.hpp"
#include "cmask.hpp"
#include "dictionary.hpp"
#include "solver.hpp"
#include "job.hpp"
#include "db.hpp"

typedef Dictionary::WordIndex WordIndex;

using std::set;
using std::vector;
using std::pair;
using std::cout;
using std::string;
using std::endl;

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
    float cutoff = 999;
    int num_turns = 0;
    vector<string> opt_dbr;
    string opt_dbw;

    po::options_description desc("Run a wordle worker that will connect to a server for work");
    desc.add_options()
        ("dbr,r",       po::value<vector<string>>(&opt_dbr),           "read-only db")
        ("dbw,w",       po::value<string>(&opt_dbw),                   "read-write db")
        ("cutoff,c",    po::value<float>(&cutoff)->default_value(999), "treat all scores at least this the same")
        ("objective,o", po::value<int>(&num_turns)->default_value(0),  "set objective to win in # turns")
        ("help,h",                                                     "produce help message");
    po::variables_map vm;
    po::parsed_options parsed = 
        po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();      
    po::store(parsed, vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cerr << desc << endl;
        return 1;
    }

    Word::test();
    Result::test();
    CMask::test();
    Solver::SolveResult::test();
    Job::test();
    Db::test();

    const vector<WordIndex>& answers = Dictionary::get_all_answers();
    const vector<WordIndex>& guesses = Dictionary::get_all_answers_and_guesses();;
    std::cout << "Loaded " << answers.size() << " answers and " << guesses.size() << " guesses" << std::endl;
    
    CMask m;
    vector<Result> res;
    WordIndex guess;

    vector<string> arguments = po::collect_unrecognized(parsed.options, po::include_positional);
    for (const std::string& rstr : arguments) {
        size_t slash = rstr.find('/') ;
        if (rstr.length() == CMask::num_hex_chars) {
            m = CMask::of_hex(rstr);
        } else if (rstr.find(' ') != string::npos || rstr.length() == 10) {
            res.push_back(Result(rstr));
        } else if (slash == string::npos) {
            if (guess != WordIndex()) {
                std::cerr << "Specified more than one guess (prev: " << *guess << ", now: " << rstr << ")" << endl;
                return 1;
            } else {
                guess = Dictionary::to_word_index(rstr);
            }
        } else {
            string answer = rstr.substr(0, slash);
            string guess = rstr.substr(slash + 1, string::npos);
            res.push_back(Result(answer, guess));
        }
    }
    for (const CMask& r : res) {
        cout << r << endl;
        m.apply(CMask(r));
    }
    cout << m << endl;
    cout << m.to_hex() << endl;
    
    std::shared_ptr<Db::Db_intf> db_ptr;
    if (opt_dbw.empty()) {
        db_ptr = std::make_shared<Db::Read_only_db>(opt_dbr);
    } else {
        db_ptr = std::make_shared<Db::Read_write_db>(opt_dbr, opt_dbw, true);
    }
    Db::Db_intf& db(*db_ptr);

    Solver::SolveResult g;
    if (num_turns > 0) {
        g = Solver::solve_b(&db, answers, guesses, m, num_turns, 0, true, true);
    } else {
        if (guess == WordIndex()) {
            g = Solver::solve_p(&db, answers, guesses, m, cutoff, true, true);
        } else {
            cout << *guess << endl;
            vector<WordIndex> valid_answers = Solver::valid_list(m, answers);
            g = Solver::solve_c(&db, valid_answers, guesses, m, guess, cutoff, true, true); 
        }
    }

    cout << "best_score   = " << g.best_score << endl;
    cout << "best_guess   = " << g.best_guess << endl;
    cout << "wost_answer  = " << g.worst_answer << endl;        
    cout << "perf_calls   = " << g.perf_calls << endl;
    cout << "perf_seconds = " << (g.perf_microseconds/1e6) << endl;
    return 0;
}
