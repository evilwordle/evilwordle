/* This now a messy pile of junk. But the idea is that each word should have a 
   canonical "pointer" so we can compare small (ie 32-bit) words "pointers" and
   put them in arrays, etc. So we statically initialize the dictionaries here
   (in an unfortunately fragile way that depends on link order).
*/

#pragma once
#include <map>
#include "word.hpp"


namespace Solver {
    void prep_max_num_results_table();
}

/* All static members here, dictionary should only init'd once per process. */
class Dictionary {
public:
    // invalidates all WordIndex's lying around. But really no reason to call this anymore
    // now that the dict is already initialized for you at at startup.
    static void init_from_file(const std::string& answers_file, const std::string& guesses_file);
    
    class WordIndex {
    public:
        const Word& operator*() const { return all_words[index]; }
        WordIndex(); //returns fake_word

        WordIndex& operator++();
        bool operator==(WordIndex other) const;
        bool operator!=(WordIndex other) const;
        bool operator<(WordIndex other) const;
    private:
        int index;
        WordIndex(int i) : index(i) {};
        friend class Dictionary;
    };
        
    static const Word& of_word_index(WordIndex i);
    static WordIndex to_word_index(const Word& w);
    static bool is_answer(WordIndex i);
    static const WordIndex fake_word;

    static const std::vector<WordIndex>& get_all_answers_and_guesses();
    static const std::vector<WordIndex>& get_all_answers();
    static const std::vector<WordIndex>& get_all_guesses();
private:
    //returns num words read
    static int load_from_file(const std::string& file);

    static std::map<Word, Dictionary::WordIndex> make_word_index_map(const std::vector<Word>& all_words);
    static std::vector<WordIndex> make_range(size_t start, size_t end);
    static void load_all_words(const char* w1[], const char* w2[]);
    static int init();    
    
    static std::vector<Word> all_words;
    static std::map<Word, WordIndex> word_index_map;
    static std::vector<WordIndex> all_answers;
    static std::vector<WordIndex> all_guesses;
    static std::vector<WordIndex> all_answers_and_guesses;
    static int num_answers;
    static int num_guesses;

    static int ignore_int_to_force_init_on_startup;

    
    friend void Solver::prep_max_num_results_table();
};
