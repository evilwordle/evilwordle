#include <fstream>
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "db.hpp"

using std::string;
using std::vector;
using std::map;
using std::cerr;
using std::endl;
using std::pair;
using boost::posix_time::ptime;
using boost::posix_time::microsec_clock;
using Solver::SolveResult;
typedef Word::Compact CompactWord;

namespace Db {
    //////////////////
    // Db_intf
    static SolveResult dummy_result;
    void Db_intf::save(const CMask& m, CompactWord g, Objective o, const SolveResult& r) { save(Job(m, g, o), r); }
    bool Db_intf::query(const CMask& m, CompactWord g, Objective o, SolveResult& r) const { return query(Job(m, g, o), r); }
    bool Db_intf::query(const CMask& m, CompactWord g, Objective o) const { return query(Job(m, g, o), dummy_result); }
    bool Db_intf::query(const Job& j) const { return query(j, dummy_result); }

    // internal to this file
    std::ostream& operator<<(std::ostream& os, const pair<Job, SolveResult>& kv) {
        os << kv.first <<  " -> " << kv.second;
        return os;
    }

    // not set externally, only used for the test.
    bool silence = false;

    //////////////////
    // Read_write_db

    Read_write_db::Read_write_db(bool debug_output_) : debug_output(debug_output_) {}
    Read_write_db::Read_write_db(const vector<string>& read_filenames, const string& write_filename, bool debug_output_) : debug_output(debug_output_) {
        for (const string& file : read_filenames) {
            load_from_file(file);
        }
        if (!write_filename.empty()) set_output_file(write_filename);
    }
    void Read_write_db::load_from_file(const string& filename) {
        ptime start = microsec_clock::local_time();
        pair<Job, SolveResult> next_record;
        std::ifstream ifs(filename, std::ios::binary);
        uint64_t count = 0;
        while(ifs.read(reinterpret_cast<char*>(&next_record), sizeof(next_record))) {
            data.insert(next_record);
            count++;
        }
        if (!silence) {
            cerr << "Read " << count << " record from db: " << filename
                 << ", took " << (microsec_clock::local_time() - start).total_microseconds() / 1e6 << "s" << endl;
        }
    }
    void Read_write_db::set_output_file(const string& filename) {
        if (output_file.is_open()) {
            throw std::runtime_error("Can't set_output_file if an output_file is already open.");
        }
    
        output_file.clear();
        output_file.open(filename, std::ios::binary | std::ios::app);
        output_file.exceptions(std::ofstream::failbit | std::ofstream::badbit);    
        if (!silence) cerr << "Writing to db: " << filename << endl;
    }
    void Read_write_db::save(const Job& j, const SolveResult& r) {
        pair<Job, SolveResult> next_record = { j, r };
        if (data.count(next_record.first)) return;
    
        data.insert(next_record);
        if (output_file.is_open() && output_file.good()) {
            if (debug_output) {
                cerr << "saving " << next_record << endl;
            }
            output_file.write(reinterpret_cast<char*>(&next_record), sizeof(next_record));
            output_file.flush();
        } else {
            if (debug_output) {
                cerr << "not saving " << next_record << endl;
            }
        }
    }

    bool Read_write_db::query(const Job& j, SolveResult& result) const {
        auto it = data.find(j);
        if (it == data.end()) {
            return false;
        } else {
            result = it->second;
            return true;
        }
    }

    //////////////////
    // ignores save commands, but is slightly faster
    // you can load from many files
    Read_only_db::Read_only_db() {};
    Read_only_db::Read_only_db(const string& filename) : Read_only_db(vector<string>{filename}) {}
    Read_only_db::Read_only_db(const vector<string>& filenames) {
        for (const string& file : filenames) {
            load_from_file_no_sort(file);
        }
        if (filenames.empty()) return;
    
        if (!silence) cerr << "Is it sorted? ...";
        ptime start = microsec_clock::local_time();
        bool is_sorted =
            std::is_sorted
            (data.begin(),
             data.end(),
             [] (const pair<Job,SolveResult>& lhs,
                 const pair<Job,SolveResult>& rhs)
             { return lhs.first < rhs.first; });
        if (is_sorted) {
            if (!silence) cerr << "yes, took " << (microsec_clock::local_time() - start).total_microseconds() / 1e6 << "s" << endl;
        } else {
            if (!silence) cerr << "no, took "  << (microsec_clock::local_time() - start).total_microseconds() / 1e6 << "s" << endl;
            sort();
        }
    }
    void Read_only_db::sort() {
        ptime start = microsec_clock::local_time();
        if (!silence) cerr << "Sorting...";
        std::sort
            (data.begin(),
             data.end(),
             [] (const pair<Job,SolveResult>& lhs,
                 const pair<Job,SolveResult>& rhs)
             { return lhs.first < rhs.first; });
        if (!silence) cerr << " done, took " << (microsec_clock::local_time() - start).total_microseconds() / 1e6 << "s" << endl;
    }
    void Read_only_db::load_from_file_no_sort(const string& filename) {
        ptime start = microsec_clock::local_time();
        std::ifstream ifs(filename, std::ios::binary | std::ifstream::ate);
        if (!(ifs.is_open() && ifs.good())) {
            cerr << "Error opening db: " << filename << endl;
            return;
        }
        size_t file_size = ifs.tellg();
        size_t num_records = (file_size / sizeof(pair<Job,SolveResult>));
        if (!silence) cerr << "Reading " << num_records << " from db: " << filename << "... ";
        ifs.seekg(0);
        size_t prev_size = data.size();
        data.resize(prev_size + num_records);

        if (!ifs.read(reinterpret_cast<char*>(&data[prev_size]), sizeof(pair<Job,SolveResult>) * num_records)) {
            cerr << " failed!";
            data.resize(prev_size);
            return;
        }
        if (!silence) cerr << " done, took " << (microsec_clock::local_time() - start).total_microseconds() / 1e6 << "s" << endl;
    }
    void Read_only_db::load_from_file(const string& filename) {
        load_from_file_no_sort(filename);
        sort();
    }
    void Read_only_db::save(const Job& j, const SolveResult& result) {
        return;
    }
    bool Read_only_db::query(const Job& j, SolveResult& result) const {
        vector<pair<Job,SolveResult>>::const_iterator it =
            std::lower_bound
            (data.begin(),
             data.end(),
             j,
             [] (const pair<Job,SolveResult>& lhs, const Job& rhs) { return lhs.first < rhs; }
             );

        if (it == data.end()) return false;
        // lower_bound guarantees thatn (it >= j). So either j < it or j == it
        if (j < it->first) return false;
        result = it->second;
        return true;
    }

    void test() {
        silence = true;
        string tmpfile = "/tmp/tmp.db.bin";
        remove(tmpfile.c_str());

        std::stringstream output1;
        std::stringstream expected1;
        std::stringstream output2;
        std::stringstream expected2;
        SolveResult s;
        s.best_score = 5;
        s.best_guess = Word("GUESS");
        s.worst_answer = Word("WORST");
        s.perf_calls = 234234;
        s.perf_microseconds = 111222333.456;
    
        CMask m1 = CMask(Word("CRATE"), Word("ABCDE"));
        CMask m2 = CMask(Word("ROATE"), Word("SOARE"));
        CMask m3 = CMask(Word("MOTEL"), Word("HOTEL"));
        Job k1(m1, Job::no_guess, Objective::adversarial);
        Job k2(m2, Word("XYZNW"), Objective::pwin2);
        Job k3(m3, Word("ABCDE"), Objective::adversarial);

        Read_write_db rw(false);
        rw.set_output_file(tmpfile);
        output1 << sizeof(Job) << endl;
        output1 << k1 << " " << rw.query(k1, s) << endl;
        output1 << k2 << " " << rw.query(k2, s) << endl;
        rw.save(k1, s);
        s.best_score = 3;
        s.best_guess = Word("AAAAZ");
        s.worst_answer = Word("BBBBB");
        s.perf_calls = 444;
        s.perf_microseconds = 4.4;
        output1 << k1 << " " << rw.query(k1, s) << " " << s << endl;
        output1 << k2 << " " << rw.query(k2, s) << endl;
        s.best_score = 7;
        s.best_guess = Word("ZZAZZ");
        s.worst_answer = Word("CCDCC");
        s.perf_calls = 333;
        s.perf_microseconds = 3.3;
        rw.save(k2, s);
        output1 << k1 << " " << rw.query(k1, s) << " " << s << endl;
        output1 << k2 << " " << rw.query(k2, s) << " " << s << endl;
        rw.output_file.close();

        expected1 << 36 << endl;
        expected1 << m1 << " o0 YYYYY 0" << endl;
        expected1 << m2 << " o2 XYZNW 0" << endl;
        expected1 << m1 << " o0 YYYYY 1 5,GUESS,WORST,234234,1.11222e+08" << endl;
        expected1 << m2 << " o2 XYZNW 0" << endl;
        expected1 << m1 << " o0 YYYYY 1 5,GUESS,WORST,234234,1.11222e+08" << endl;
        expected1 << m2 << " o2 XYZNW 1 7,ZZAZZ,CCDCC,333,3.3" << endl;
    
        std::string output1_str = output1.str();
        std::string expected1_str = expected1.str();
        if (output1_str != expected1_str) {
            remove(tmpfile.c_str());
            throw std::runtime_error("Db::test() 1 failed, got " + output1_str + ", but expected " + expected1_str);
        }

        Read_only_db ro({tmpfile});
        output2 << k1 << " " << ro.query(k1, s) << " " << s << endl;
        output2 << k2 << " " << ro.query(k2, s) << " " << s << endl;
        output2 << k3 << " " << ro.query(k3, s) << " " << s << endl;

        expected2 << m1 << " o0 YYYYY 1 5,GUESS,WORST,234234,1.11222e+08" << endl;
        expected2 << m2 << " o2 XYZNW 1 7,ZZAZZ,CCDCC,333,3.3" << endl;
        expected2 << m3 << " o0 ABCDE 0 7,ZZAZZ,CCDCC,333,3.3" << endl;

        remove(tmpfile.c_str());
        std::string output2_str = output2.str();
        std::string expected2_str = expected2.str();
        if (output2_str != expected2_str) {
            throw std::runtime_error("Db::test() 2 failed, got\n" + output2_str + ", but expected\n" + expected2_str);
        }   
        silence = false;
    }

}
