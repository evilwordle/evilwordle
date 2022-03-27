#include <sstream>
#include "job.hpp"

std::ostream& operator<<(std::ostream& os, Objective o) {
    return os << "o" << static_cast<int>(o);
}

Objective objective_of_string(const std::string& str) {
    size_t l = str.length();
    if (l > 0) {
        char c = str[l - 1];
        if (c >= '0' && c <= '5') {
            return static_cast<Objective>((int)(c - '0'));
        }
    }
    throw std::runtime_error("objective_of_string: " + str);
}


const Word::Compact Job::no_guess = Word("YYYYY");

int32_t pack_guess_and_objective(Word::Compact g, Objective o) {
    int32_t gv = *(reinterpret_cast<const int32_t*>(&g));
    return gv | (static_cast<int32_t>(o) << 25);
}

Word::Compact Job::get_guess() const {
    int32_t gv = guess_and_objective & 0x01FFFFFF;
    return *(reinterpret_cast<const Word::Compact*>(&gv));
}

Objective Job::get_objective() const {
    return static_cast<Objective>(guess_and_objective >> 25);
}

const CMask& Job::get_mask() const {
    return mask;
}


void Job::set_mask(const CMask& m) {
    mask = m;
}
void Job::apply_mask(const CMask& m) {
    mask.apply(m);
}
void Job::set_guess(Word::Compact g) {
    guess_and_objective = pack_guess_and_objective(g, get_objective());
}
void Job::set_objective(Objective o) {
    guess_and_objective = pack_guess_and_objective(get_guess(), o);
}

Job::Job() :
    mask(),
    guess_and_objective(pack_guess_and_objective(no_guess, Objective::adversarial))
{};

Job::Job(const CMask& mask_, Word::Compact guess_, Objective objective_) :
    mask(mask_),
    guess_and_objective(pack_guess_and_objective(guess_, objective_))
{};

bool Job::operator<(const Job& k) const {
    if (mask < k.mask) return true;
    if (k.mask < mask) return false;
    return guess_and_objective < k.guess_and_objective;
};

std::ostream& operator<<(std::ostream& os, const Job& k) {
    os << k.get_mask() << " " << k.get_objective() << " " << k.get_guess();
    return os;
}

void Job::test() {
    Job j0(CMask(), Word("ABCDE"), Objective::adversarial);
    Job j1(CMask(), Word("XYZWA"), Objective::pwin1);
    Job j2(CMask(), Word("ZAAAA"), Objective::pwin2);
    Job j3(CMask(), Word("ABABA"), Objective::pwin4);
    Job j4(CMask(), no_guess,      Objective::pwin5);

    std::stringstream output;
    std::stringstream expected;
    output << j0.get_objective() << " " << j0.get_guess() << std::endl;
    output << j1.get_objective() << " " << j1.get_guess() << std::endl;
    output << j2.get_objective() << " " << j2.get_guess() << std::endl;
    output << j3.get_objective() << " " << j3.get_guess() << std::endl;
    output << j4.get_objective() << " " << j4.get_guess() << std::endl;
    expected << "o0" << " " << "ABCDE" << std::endl;
    expected << "o1" << " " << "XYZWA" << std::endl;
    expected << "o2" << " " << "ZAAAA" << std::endl;
    expected << "o4" << " " << "ABABA" << std::endl;
    expected << "o5" << " " << "YYYYY" << std::endl;

    std::string output_str = output.str();
    std::string expected_str = expected.str();
    if (output_str != expected_str) {
        throw std::runtime_error("Job::test()  failed, got " + output_str + ", but expected " + expected_str);
    }
}
