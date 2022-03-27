#include <sstream>
#include <array>
#include "word.hpp"

// once letters is set, populate all_letters, letter_masks, and compact

void Word::prepare_masks() {
    static std::array<char, 32> counts;
    std::fill(counts.begin(), counts.end(), 0);
    
    all_letters = 0;
    all_double_letters = 0;
    all_triple_letters = 0;
    int32_t c = 0;
    for (int i=0; i<5; i++) {
        int p = letters[i] - 'A';
        int m = 1 << p;

        int new_count = ++counts[p];
        all_letters |= m;
        if (new_count >= 2) all_double_letters |= m;
        if (new_count >= 3) all_triple_letters |= m;
        
        c <<= 5;
        c |= p;            
    }
    
    compact = c;    
    cmask = CMask(letters);
}

Word::Word(const std::string& r) {
    int count = 0;
    for (char c : r) {
        if (c >= 'a' && c <= 'z' && count < 5) {
            letters[count] = c + 'A' - 'a';
            count++;
        } else if (c >= 'A' && c <= 'Z' && count < 5) {
            letters[count] = c;
            count++;
        }
    }
    if (count != 5) {
        throw std::runtime_error("Expected 5 letter word, not: " + r);
    }

    prepare_masks();    
}

Word::Word(Word::Compact compact) {
    int32_t c = compact.c;
    letters[4] = (c & 0x1F) + 'A';
    c >>= 5;
    letters[3] = (c & 0x1F) + 'A';
    c >>= 5;
    letters[2] = (c & 0x1F) + 'A';
    c >>= 5;
    letters[1] = (c & 0x1F) + 'A';
    c >>= 5;
    letters[0] = (c & 0x1F) + 'A';

    prepare_masks();
}

bool Word::operator==(const Word& r) const {
    return (compact.c == r.compact.c);
}

bool Word::operator<(const Word& r) const {
    return (compact.c < r.compact.c);
}

std::ostream& operator<<(std::ostream& os, const Word& x) {
    return os.write(x.letters, 5);
}

void Word::test() {
    Word x ("azZAq");
    std::stringstream output1;
    std::stringstream output2;
    std::stringstream output3;
    std::stringstream expected;
    std::stringstream expected3;
    
    output1 << x << " "
              << x.all_letters << " "
              << x.compact.c << " "
              << sizeof(x) << " "
              << sizeof(x.compact)
              << std::endl;

    Word y = Word::Compact(x);
    output2 << y << " "
              << y.all_letters << " "
              << y.compact.c << " "
              << sizeof(y) << " "
              << sizeof(y.compact)
              << std::endl;


    expected << "AZZAQ" << " "
              << int32_t(1<<0) + (1<<16) + (1<<25) << " "
              << int32_t(0<<20) +(25<<15) + (25<<10) +(0<<5) + (16<<0) << " "
              << 64 << " "
              << 4
              << std::endl;


    std::string output1_str = output1.str();
    std::string output2_str = output2.str();
    std::string expected_str = expected.str();
    if (output1_str != expected_str) {
        throw std::runtime_error("Word::test() 1 failed, got " + output1_str + ", but expected " + expected_str);
    }
    if (output2_str != expected_str) {
        throw std::runtime_error("Word::test() 2 failed, got " + output2_str + ", but expected " + expected_str);
    }

    Word z1("EAAZE");
    Word z2("ROROR");
    output3 << z1 << " "
            << z1.all_letters << " "
            << z1.all_double_letters << " "
            << z1.all_triple_letters << " "
            << z2 << " "
            << z2.all_letters << " "
            << z2.all_double_letters << " "
            << z2.all_triple_letters << " ";

    expected3 << "EAAZE" << " "
              << int32_t(1<<0) + (1<<4) + (1<<25) << " "
              << int32_t(1<<0) + (1<<4) << " "
              << 0 << " "
              << "ROROR" << " "
              << int32_t(1<<14) + (1<<17) << " "
              << int32_t(1<<14) + (1<<17) << " "
              << int32_t(1<<17) << " ";
    
    std::string output3_str = output3.str();
    std::string expected3_str = expected3.str();
    if (output3_str != expected3_str) {
        throw std::runtime_error("Result::test() 3 failed, got " + output3_str + ", but expected " + expected3_str);
    }
}
