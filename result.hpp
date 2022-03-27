/* Represents a filled in row in the game, ie. the information gained from testing
   a guess. Basically the guess + a sequence of 5x (green/yellow/black).
   
   We don't actually use this anywhere in the fast path anymore (and now just construct 
   the CMasks directly from the answer and guess). But it's useful for human input and
   for the website to specify these rather than CMasks. 
*/

#pragma once
#include "word.hpp"

class Result {
public:
    // format is ABCDE __.~_
    // means C is green and D is yellow
    // All characters are URL-encode-safe.
    Result(const std::string& r);

    // an impossible result
    Result();    
    
    Result(const Word& answer, const Word& guess);
    
    // returns one of black/yellow/green below
    char get_result(int i) const;
    char get_letter(int i) const;
    
    static const char black;
    static const char yellow;
    static const char green;
    
    bool operator<(Result r)  const;
    bool operator==(Result r) const;

	int num_yellow() const;
	int num_green() const;
	int num_black() const;

    static void test();
private:    
    // these are only used in the string input representation
    static const char black_char;
    static const char yellow_char;
    static const char green_char;
    union {
        int64_t all;
#pragma pack(push,1)
        struct {
            char letter[5];
            int16_t result;
        };
#pragma pack(pop)
    };
};

std::ostream& operator<<(std::ostream& os, const Result& x);
