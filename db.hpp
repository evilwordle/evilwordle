#pragma once
#include <map>
#include <vector>
#include <set>
#include <fstream>
#include "word.hpp"
#include "solveresult.hpp"
#include "job.hpp"

namespace Db {
    class Db_intf {    
    public:
        virtual ~Db_intf() {};
        virtual void save(const Job& j, const Solver::SolveResult& result) = 0;
        // returns true if we found the answer cached in the db. If we return false [result] was not touched.
        virtual bool query(const Job& j, Solver::SolveResult& result) const = 0;    

        // other overloads
        void save(const CMask& mask, Word::Compact guess, Objective obj, const Solver::SolveResult& result);
        bool query(const CMask& mask, Word::Compact guess, Objective obj, Solver::SolveResult& result) const;    
        bool query(const CMask& mask, Word::Compact guess, Objective obj) const;
        bool query(const Job& j) const;
    };

    class Read_write_db : public Db_intf {
    public:
        // you can load from many files
        void load_from_file(const std::string& filename);

        // you can only write to one file, and this fails if there's already an output file open
        void set_output_file(const std::string& filename);

        // you can load from many files
        Read_write_db(bool debug_output);
        Read_write_db(const std::vector<std::string>& read_filenames, const std::string& write_filename, bool debug_output);

        virtual void save(const Job& j, const Solver::SolveResult& result);
        virtual bool query(const Job& j, Solver::SolveResult& result) const;    
    private:
        std::map<Job, Solver::SolveResult> data;
        std::ofstream output_file;
        bool debug_output;
    
        friend void test();    
    };

    // ignores save commands, but is slightly faster to load/use because of flat-array storage
    class Read_only_db : public Db_intf {
    public:
        // you can load from many files
        Read_only_db();
        Read_only_db(const std::string& filename);
        Read_only_db(const std::vector<std::string>& filenames);
    
        void load_from_file(const std::string& filename);

        virtual void save(const Job& j, const Solver::SolveResult& result);
        virtual bool query(const Job& j, Solver::SolveResult& result) const;    
    private:
        void load_from_file_no_sort(const std::string& filename);
        void sort();

        std::vector<std::pair<Job, Solver::SolveResult>> data;
    };

    void test();
}
