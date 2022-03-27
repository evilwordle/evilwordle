//#include <boost/lexical_cast.hpp>
#include <fstream>
#include <vector>
#include <map>
#include "dictionary.hpp"
#include "dictionary_static.h"

using std::string;
using std::cerr;
using std::endl;
using std::vector;
using std::map;
using std::pair;

void Dictionary::init_from_file(const string& answers_file, const string& guesses_file) {
    all_words.clear();
    
    all_words.push_back(Word("ZZZZZ")); // fake_word
    num_answers = load_from_file(answers_file);
    num_guesses = load_from_file(guesses_file);
    word_index_map = make_word_index_map(all_words);
    all_answers = make_range(1, num_answers + 1);
    all_guesses = make_range(num_answers + 1, num_answers + num_guesses + 1);
    all_answers_and_guesses =  make_range(1, num_answers + num_guesses + 1);
    cerr << "Loaded " << num_answers << " answers and " << num_guesses << " non-answer guesses." << endl;
}

Dictionary::WordIndex Dictionary::to_word_index(const Word& w) {
    auto it = word_index_map.find(w);
    if (it == word_index_map.end()) {
        throw std::runtime_error("Word not in dictionary");
        //+ boost::lexical_cast<string>(w));
    }
    return it->second;                
}

const Word& Dictionary::of_word_index(Dictionary::WordIndex i) {
    return all_words[i.index];
}

bool Dictionary::is_answer(Dictionary::WordIndex i) {
    return i.index > 0 && i.index <= num_answers;
}

//returns num words read
int Dictionary::load_from_file(const string& filename) {
    std::ifstream f(filename.c_str());
    string line;
    int count = 0;
    while(std::getline(f, line)) {
        if (line.length () > 1) {
            all_words.push_back(Word(line));
            count++;
        }
    }
    return count;
}

Dictionary::WordIndex& Dictionary::WordIndex::operator++() {
    ++index;
    return *this;
}
bool Dictionary::WordIndex::operator==(WordIndex other) const {
    return index == other.index;
}
bool Dictionary::WordIndex::operator!=(WordIndex other) const {
    return index != other.index;
}
bool Dictionary::WordIndex::operator<(WordIndex other) const {
    return index < other.index;
}
Dictionary::WordIndex::WordIndex() : index(0) {}


const vector<Dictionary::WordIndex>& Dictionary::get_all_answers_and_guesses() { return all_answers_and_guesses; }
const vector<Dictionary::WordIndex>& Dictionary::get_all_answers() { return all_answers; }
const vector<Dictionary::WordIndex>& Dictionary::get_all_guesses() { return all_guesses; }


map<Word, Dictionary::WordIndex>
             Dictionary::make_word_index_map(const vector<Word>& all_words)
{
    map<Word, Dictionary::WordIndex> rv;
    // intentionally start at 1 to skip the fake word
    for (size_t i = 1 ; i < all_words.size() ; i++) {
        rv.insert(pair<Word, WordIndex>(all_words[i], WordIndex(i)));
    }
    return rv;
}   

vector<Dictionary::WordIndex> Dictionary::make_range(size_t start, size_t end) {
    vector<WordIndex> rv;
    rv.reserve(end - start);
    for (size_t i = start; i != end; i++) {
        rv.push_back(WordIndex(i));
    }
    return rv;
}

// all_words[0] = fake_word
// all_words[1] ... all_words[num_answers] are answers
// all_words[num_answers + 1] ... all_words[num_answers + num_guesses] are guesses

//////////////////////////////////////
// this was when we didn't init statically.

vector<Word> Dictionary::all_words;
std::map<Word, Dictionary::WordIndex> Dictionary::word_index_map;
int Dictionary::num_answers = 0;
int Dictionary::num_guesses = 0; 
vector<Dictionary::WordIndex> Dictionary::all_answers_and_guesses;
vector<Dictionary::WordIndex> Dictionary::all_answers;
vector<Dictionary::WordIndex> Dictionary::all_guesses;
const Dictionary::WordIndex Dictionary::fake_word = WordIndex(0);

//////////////////////////////////////
// this is for static initialization

void Dictionary::load_all_words(const char* w1[], const char* w2[]) {
    all_words.clear();
    all_words.push_back(Word("ZZZZZ")); // fake_word
    num_answers = 0;
    num_guesses = 0;
    while (**w1) {
        all_words.push_back(Word(string(*w1)));
        w1++;
        num_answers++;
    }

    while (**w2) {
        all_words.push_back(Word(string(*w2)));
        w2++;
        num_guesses++;
    }
}

int Dictionary::init() {
    load_all_words(raw_answers, raw_guesses);
    word_index_map = make_word_index_map(all_words);
    all_answers = make_range(1, num_answers + 1);
    all_guesses = make_range(num_answers + 1, num_answers + num_guesses + 1);
    all_answers_and_guesses = make_range(1, num_answers + num_guesses + 1);
    return 0;
}

int Dictionary::ignore_int_to_force_init_on_startup = Dictionary::init();
