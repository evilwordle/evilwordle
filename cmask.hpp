/********

A CMask represents the state of the game. A CMask can tell you if a given anwer is valid (via the [check] function).

Internally a CMask stores information about what greens, yellows, and blacks you've seen and in what positions.

You can create a CMask using a guess/answer pair, or the equivalent Result, but it's faster to do it directly using
the guess/answer pair and skip the intermediate step. You can also compose multiple CMasks together via [apply].

Our internal representation is 26 bytes, but we have to pad to 32 to safely use fast AVX/NEON instructions. Three
functions here are performance critical as perf shows:

  37.25% CMask::check
  27.68% Solver::valid_list
   7.95% CMask::CMask
   4.16% CMask::operator<
   4.07% Solver::solve_c
   3.22% Solver::solve_p

Note: we rely on the fact that no word in the wordle dictionary has four of the same letter. We can only store up
to three of the same leter.

*******/


#pragma once
#include <iostream>
#ifdef __x86_64
  #define USE_AVX 1
#else
  #define USE_AVX 0
#endif
#if __aarch64__
  #define USE_NEON 1
#else
  #define USE_NEON 0
#endif

class Word;
class Result;

class SingleCharData {
public:
    SingleCharData apply(SingleCharData other);

    bool get_count_exact() const;
    uint8_t get_count_min() const;
    uint8_t get_disallowed_pos() const;
private:
    union {
        uint8_t all;
        struct {
            /*
              Internally we put a 1 in the LSB [disallowed_pos] if we know a letter can't be in the left-most pos.
              We put a 1 in bit #X if we know it cannot be in X (X = 0 is the left, X = 4 is the right).
              
              count_min is the minimum number of that letter known to be in the word (including greens). Can be up to 3.

              count_exact = 1 if we know that count_min is also a max (because we've gotten a black as well, for example
              two yellows and a black means it's *exactly* two, so count_min=2, count_exact=true).              

             */
            uint8_t disallowed_pos : 5;
            uint8_t count_min      : 2;
            uint8_t count_exact    : 1;
        };
    };

    friend class CMask;
};

typedef unsigned char v32c __attribute__ ((vector_size (32)));

class CMask {
public:
    // initial state of game, anything is allowed. Hex/binary representation is all zero.
    CMask();

    // intended to be fast
    CMask(const Word& answer, const Word& guess);
    
    // Not an actual mask. We set disallowed_pos mostly 0, but 1 where where it exists,
    // count_exact = false. used to construct the cmask inside Word. This function is not
    // intended to be fast or used directly in solver, but makes it faster to run [check]
    // against known words where this is precomputed.
    CMask(const char letters[5]);

    // not intended to be fast or used directly in solver.
    CMask(const Result& r);
    
    // logic operations
    CMask& apply(const CMask& m); // intended to be fast
    bool check(const Word& w) const; // [check] is the most speed-critical function
    bool operator<(const CMask& m) const;
    bool operator==(const CMask& m) const;
    bool has_at_most_one_letter_undetermined() const;
    bool has_at_least_one_green() const; // might be wrong beyond turn 5
    int count_yellow_or_green(const bool* is_letter_valid = nullptr) const; //is_letter_valid should be an array 0..25
    size_t hash() const;

    // very slow, but raises a message that's useful for the user.
    void check_detail_reasons_exn(const Word& w) const;
    
    // IO/other operations
    SingleCharData get_single_char(char letter) const; //letter must be in 'A'..'Z'
    std::string to_hex() const;
    static CMask of_hex(const std::string& s);
    friend std::ostream& operator<<(std::ostream& os, const CMask& m);
    static void test();
    
    // len of to_hex/of_hex strings. Genreally 2x number of bytes in binary representation on CMask
    // excluding the padding.
    static const unsigned int num_hex_chars;
private:    
    static const uint64_t w012_mask_remove_all_by_pos[];
    static const uint16_t w3_mask_remove_all_by_pos[];
#pragma pack(push,1)
    union {
        SingleCharData c[26];
        struct {
            uint64_t w0;
            uint64_t w1;
            uint64_t w2;
            uint16_t w3;
        };
        v32c v;
    };
#pragma pack(pop)

    void output(std::ostream& os, bool ansi_escapes) const;
};

std::ostream& operator<<(std::ostream& os, const CMask& m);
