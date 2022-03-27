#include <sstream>
#include <array>
#include "result.hpp"

Result::Result(const std::string& r) {
    int lcount = 0;
    int rcount = 0;
    all = 0;
    for (char c : r) {
        if (c >= 'a' && c <= 'z' && lcount < 5) {
            letter[lcount] = c + 'A' - 'a';
            lcount++;
        } else if (c >= 'A' && c <= 'Z' && lcount < 5) {
            letter[lcount] = c;
            lcount++;
        } else if (c == black_char && rcount < 5) {
            result |= (black << (rcount * 2));
            rcount++;
        } else if (c == yellow_char && rcount < 5) {
            result |= (yellow << (rcount * 2));
            rcount++;
        } else if (c == green_char && rcount < 5) {
            result |= (green << (rcount * 2));
            rcount++;
        }
    }
    if (lcount != 5 || rcount != 5) {
        throw std::runtime_error("Expected 5 letter word and 5 results, not: " + r);
    }
}
    
Result::Result() {
    all = 0;
}
    
Result::Result(const Word& answer, const Word& guess) {
    int32_t matched_one_already = 0;
    int32_t matched_two_already = 0;

    all = 0;
    int32_t black_letters = 0;

    for (int i = 0 ; i < 5; i++) {
        letter[i] = guess.letters[i];
        
        if (guess.letters[i] == answer.letters[i]) {
            result |= (green << (i * 2));

            int m = 1 << (guess.letters[i] - 'A');
            if (matched_one_already & m) {
                matched_two_already |= m;
            } else {
                matched_one_already |= m;
            }
        }
    }
    
    for (int i = 0 ; i < 5; i++) {
        int32_t m = 1 << (guess.letters[i] - 'A');

        int32_t mask_to_use;
        int32_t* mask_to_update;
        if (matched_two_already & m) {
            mask_to_use = answer.all_triple_letters;
            mask_to_update = &mask_to_use; // some garbage thing, we don't need to keep track because nothing matches 4 times
        } else if (matched_one_already & m) {
            mask_to_use = answer.all_double_letters;
            mask_to_update = &matched_two_already;
        } else {
            mask_to_use = answer.all_letters;
            mask_to_update = &matched_one_already;
        }

        if (guess.letters[i] == answer.letters[i]) {
            // already handled
        } else if (m & mask_to_use) {
            result |= (yellow << (i * 2));
            (*mask_to_update) |= m;            
        } else {
            black_letters |= m;
            result |= (black << (i * 2));
        }
    }
}

int Result::num_yellow() const {
	int i=0;
	for (int j=0; j<5; j++) {
		if (get_result(j) == yellow) i++;	
	}
	return i;
};
int Result::num_black() const {
	int i=0;
	for (int j=0; j<5; j++) {
		if (get_result(j) == black) i++;	
	}
	return i;
};
int Result::num_green() const {
	int i=0;
	for (int j=0; j<5; j++) {
		if (get_result(j) == green) i++;	
	}
	return i;
};

char Result::get_result(int i) const { return (result >> (i * 2)) & 3; }
char Result::get_letter(int i) const { return letter[i]; }

const char Result::black = 1;
const char Result::yellow = 2;
const char Result::green = 3;

bool Result::operator<(Result r)  const { return  all < r.all; } 
bool Result::operator==(Result r) const { return  all == r.all; } 

const char Result::black_char  = '_';
const char Result::yellow_char = '~';
const char Result::green_char  = '.';

std::ostream& operator<<(std::ostream& os, const Result& x) {
    for (int i = 0; i < 5 ; i++) {
        if (x.get_result(i) == Result::black) {
            os << "\033[37;40m" << x.get_letter(i);
        } else if (x.get_result(i) == Result::yellow) {
            os << "\033[30;43m" << x.get_letter(i);
        } else if (x.get_result(i) == Result::green) {
            os << "\033[30;42m" << x.get_letter(i);
        }
    }
    return (os << "\033[0m");
}


void Result::test() {
    Word x ("ABCDE");
    Word y ("ZAFDL");
    
    std::stringstream output1;
    std::stringstream output2;
    std::stringstream expected1;
    std::stringstream expected2;
    
    output1 << Result(x,y).all << " "
            << Result(x,y).letter[0]
            << Result(x,y).letter[1]
            << Result(x,y).letter[2]
            << Result(x,y).letter[3]
            << Result(x,y).letter[4]
            << " "
            << Result("ABCDE _____").all << " "
            << Result("EFGHI ._.._").all << " "
            << Result("JKLMN ~__~~").all << std::endl;

    expected1 << "520396562907482 ZAFDL 375230963073601 553369094014533 728213001882442" << std::endl;

    std::string output1_str = output1.str();
    std::string expected1_str = expected1.str();
    if (output1_str != expected1_str) {
        throw std::runtime_error("Result::test() 1 failed, got " + output1_str + ", but expected " + expected1_str);
    }    

    output2   << Result(Word("JAUNT"), Word("TAUNT")) << std::endl;
    output2   << Result(Word("BRINE"), Word("SWEET")) << std::endl;
    output2   << Result(Word("AABBB"), Word("BBAAA")) << std::endl;
    output2   << Result(Word("MUMMY"), Word("MMYYM")) << std::endl;
    output2   << Result(Word("PHOTO"), Word("FLOOD")) << std::endl;
    output2   << Result(Word("WROTE"), Word("FLOOD")) << std::endl;
    output2   << Result(Word("BONGO"), Word("FLOOD")) << std::endl;
    output2   << Result(Word("YOUTH"), Word("FLOOD")) << std::endl;
    output2   << Result(Word("WORLD"), Word("FLOOD")) << std::endl;

    expected2 << Result("TAUNT _....") << std::endl;
    expected2 << Result("SWEET __~__") << std::endl;
    expected2 << Result("BBAAA ~~~~_") << std::endl;
    expected2 << Result("MMYYM .~~_~") << std::endl;
    expected2 << Result("FLOOD __.~_") << std::endl;
    expected2 << Result("FLOOD __.__") << std::endl;
    expected2 << Result("FLOOD __~~_") << std::endl;
    expected2 << Result("FLOOD __~__") << std::endl;
    expected2 << Result("FLOOD _~~_.") << std::endl;
    
    std::string output2_str = output2.str();
    std::string expected2_str = expected2.str();
    if (output2_str != expected2_str) {
        throw std::runtime_error("Result::test() 2 failed, got\n" + output2_str + ", but expected\n" + expected2_str);
    }    

}
