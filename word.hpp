/* Represents a five-letter words. It also precomputes a bunch of stuff that's useful
   for Result and CMask later.

   Each Word contains the char characters, but also a "Compact" representation in 4 bytes
   (5 bits * 5 letters = 25 bits, so we don't even use the top 6 bits).

   Creating a word is expensive. When actually solving the words are already created and 
   stored in an array (see dictionary.hpp).
*/

#pragma once
#include <string>
#include <iostream>
#include "cmask.hpp"

class Word {
public:
    Word(const std::string& r);
    bool operator==(const Word& r) const;    
    bool operator<(const Word& r) const;
    char operator[](int pos) const { return letters[pos]; }
    friend std::ostream& operator<<(std::ostream& os, const Word& x);
    friend class Result;
    friend class Mask;
    friend class CMask;

    class Compact {
    public:
        Compact(const Word& w) : c(w.compact.c) {}
        bool operator<(Compact o) const { return c < o.c; }
        bool operator==(Compact o) const { return c == o.c; }
    private:
        Compact() : c(0) {}
        Compact(int32_t c_) : c(c_) {}
        int32_t c;
        friend class Word;
    };
    
    Word(Compact cw);

    static void test();
    void prepare_masks();
private:
    CMask cmask;     //32
    Compact compact; // 4
    int32_t all_letters; // 4
    int32_t all_double_letters; // 4
    int32_t all_triple_letters; // 4

    char letters[5]; // 5
    
    char padding[11]; // gets the total to 64 bytes
};

std::ostream& operator<<(std::ostream& os, const Word& x);
