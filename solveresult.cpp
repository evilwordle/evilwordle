#include <sstream>
#include <boost/lexical_cast.hpp>
#include "solveresult.hpp"

using std::string;
using std::stringstream;

namespace Solver {
    std::ostream& operator<<(std::ostream& os, const SolveResult& s) {
        os << s.best_score << "," << s.best_guess << "," << s.worst_answer << "," << s.perf_calls << "," << s.perf_microseconds;
        return os;
    }

    std::string SolveResult::to_string() const{
        stringstream ss;
        ss << *this;
        return ss.str();
    }

    SolveResult SolveResult::of_string(const std::string& r) {
        SolveResult t;
        stringstream ss(r);
        string g;
        std::getline(ss, g, ',');
        t.best_score = boost::lexical_cast<float>(g);
        std::getline(ss, g, ',');
        t.best_guess = Word::Compact(Word(g));
        std::getline(ss, g, ',');
        t.worst_answer = Word::Compact(Word(g));
        std::getline(ss, g, ',');
        t.perf_calls = boost::lexical_cast<float>(g);
        std::getline(ss, g, ',');
        t.perf_microseconds = boost::lexical_cast<float>(g);
        return t;
    }

    void SolveResult::test() {
        SolveResult s;
        s.best_score = 4.5;
        s.best_guess = Word("GUESS");
        s.worst_answer = Word("WORST");
        s.perf_calls = 234234;
        s.perf_microseconds = 111222333.456;

        std::stringstream output1;
        std::stringstream expected;
        
        output1
            << s << std::endl
            << of_string(s.to_string()) << std::endl;
        
        s.perf_calls = 1.234e+07;
        s.perf_microseconds = 1;
        
        output1
            << s << std::endl
            << of_string(s.to_string()) << std::endl
            << sizeof(SolveResult) << std::endl;
	
        expected
            << "4.5,GUESS,WORST,234234,1.11222e+08" << std::endl
            << "4.5,GUESS,WORST,234234,1.11222e+08" << std::endl
            << "4.5,GUESS,WORST,1.234e+07,1" << std::endl
            << "4.5,GUESS,WORST,1.234e+07,1" << std::endl
            << "20" << std::endl;

        std::string output1_str = output1.str();
        std::string expected_str = expected.str();
        if (output1_str != expected_str) {
            throw std::runtime_error("SolveResult::test() 1 failed, got " + output1_str + ", but expected " + expected_str);
        }    
    }
}
