#include <sstream>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "cmask.hpp"
#include "word.hpp"
#include "result.hpp"
#include "dictionary.hpp"
#if USE_AVX
#include <immintrin.h>
#elif USE_NEON
#include <arm_neon.h>
#endif

SingleCharData SingleCharData::apply(SingleCharData other) {
    SingleCharData rv;
    rv.all = std::max(all & 0x60, other.all & 0x60) | ((all | other.all) & 0x9F);
    return rv;
};

bool SingleCharData::get_count_exact() const {
    return count_exact;
}
uint8_t SingleCharData::get_count_min() const {
    return count_min;
}
uint8_t SingleCharData::get_disallowed_pos() const {
    return disallowed_pos;
}
typedef unsigned char v32c __attribute__ ((vector_size (32)));

CMask::CMask() : v{0,0,0,0} {}

// disallowed_pos mostly 0, but 1 where where it exists, count_exact = false. used to construct
// the cmask inside Word
CMask::CMask(const char letters[5]) : v{0,0,0,0} {
    for (int i = 0; i < 5; i++) {
        int zc = letters[i] - 'A';
        if (zc < 0 || zc >=26) throw std::runtime_error("bad Word passed to  CMask: " + std::string(letters, letters+5));
        c[zc].count_min++;
        c[zc].disallowed_pos |= (1 << i);
    }
}

const uint64_t CMask::w012_mask_remove_all_by_pos[] =
    { 0x0101010101010101,
      0x0202020202020202,
      0x0404040404040404,
      0x0808080808080808,
      0x1010101010101010 
    };

const uint16_t CMask::w3_mask_remove_all_by_pos[] =
    { 0x0101,
      0x0202,
      0x0404,
      0x0808,
      0x1010
    };

CMask::CMask(const Word& answer, const Word& guess) : v{0,0,0,0} {        
    int32_t green_1 = 0;
    int32_t green_2 = 0;
    int32_t green_3 = 0;
    
    for (int i = 0; i < 5; i++) {
        int zc = guess[i] - 'A';
        //if (zc < 0 || zc >=26) throw std::runtime_error("bad guess passed to CMask(answer,guess)");

        if (answer[i] == guess[i]) {
            w0 |= w012_mask_remove_all_by_pos[i];
            w1 |= w012_mask_remove_all_by_pos[i];
            w2 |= w012_mask_remove_all_by_pos[i];
            w3 |= w3_mask_remove_all_by_pos[i];
            c[zc].all &= ~(1 << i);

            int32_t m = 1 << zc;
            green_3 |= (green_2 & m);
            green_2 |= (green_1 & m);
            green_1 |= m;
        }
    }
    for (int i = 0; i < 5; i++) {
        int zc = guess[i] - 'A';
        uint8_t answer_cmin = answer.cmask.c[zc].all & 0x60;
        uint8_t guess_cmin  = guess.cmask.c[zc].all & 0x60;

        if (answer[i] != guess[i] && answer_cmin > 0) {
            int32_t green_count =
                ((green_3 >> zc) & 1) +
                ((green_2 >> zc) & 1) +
                ((green_1 >> zc) & 1);

            if ((answer_cmin >> 5) > green_count) {
                // We only mark positions if it's necessary (because we have a yellow). Otherwise we could generate
                // logically equivalent masks that have inconsequential differences only in disallowed_by_pos compare
                // as unequal, ruining our caching.
                c[zc].all |= (1 << i);
            }
        }

        if (guess_cmin > answer_cmin) {
            c[zc].all |= 0x80 | answer_cmin;
        } else {
            c[zc].all |= guess_cmin;
        }
    }
}

CMask::CMask(const Result& r) : v{0,0,0,0} {
    int32_t has_yellow = 0;

    for (int i = 0; i < 5; i++) {
        int zc = r.get_letter(i) - 'A';
        char result = r.get_result(i);
        if (result == Result::green) {
            w0 |= w012_mask_remove_all_by_pos[i];
            w1 |= w012_mask_remove_all_by_pos[i];
            w2 |= w012_mask_remove_all_by_pos[i];
            w3 |= w3_mask_remove_all_by_pos[i];
            c[zc].all &= ~(1 << i);
            c[zc].count_min++;
        } else {
            if (result == Result::yellow) {
                has_yellow |= (1 << zc);
                c[zc].count_min++;
            } else { // result = black
                c[zc].all |= 0x80;
            }
        }
    }
    for (int i = 0; i < 5; i++) {
        int zc = r.get_letter(i) - 'A';
        char result = r.get_result(i);
        if (result != Result::green && (has_yellow & (1 << zc))) {
            c[zc].all |= (1 << i);
        }
    }
}

static const uint64_t w012_mask_disallowed_pos = 0x1F1F1F1F1F1F1F1F;
static const uint64_t w3_mask_disallowed_pos = 0x1F1F;

static const uint64_t w012_mask_count_min = 0x6060606060606060;
static const uint64_t w3_mask_count_min = 0x6060;

static const uint64_t w012_mask_count_exact = 0x8080808080808080;
static const uint64_t w3_mask_count_exact = 0x8080;

static const v32c v_mask_disallowed_pos =
    {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,
     0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,
     0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,
     0x1F,0x1F,0   ,0   ,0   ,0   ,0   ,0};

static const v32c v_mask_count_min =
    {0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,
     0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,
     0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,
     0x60,0x60,0   ,0   ,0   ,0   ,0   ,0};

static const v32c v_mask_count_exact =
    {0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
     0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
     0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
     0x80,0x80,0   ,0   ,0   ,0   ,0   ,0};

static const v32c v_mask_exact_and_disallowed =
    {0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,
     0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,
     0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,
     0x9F,0x9F,0   ,0   ,0   ,0   ,0   ,0};

#if USE_AVX
union simd_vector {
  v32c    v;
  __m256i y;
};
#elif USE_NEON
union simd_vector {
  v32c    v;
  struct {
    uint8x16_t y0;
    uint8x16_t y1;
  };
  struct {
    uint64x2_t yw0;
    uint64x2_t yw1;
  };
};


const simd_vector sv_mask_disallowed_pos = { v_mask_disallowed_pos };
const simd_vector sv_mask_count_min = { v_mask_count_min };
const simd_vector sv_mask_count_exact = { v_mask_count_exact };
const simd_vector sv_mask_exact_and_disallowed = { v_mask_exact_and_disallowed };

#endif

bool CMask::check(const Word& w) const {
    const CMask& other = w.cmask;

    #if USE_AVX    
    // Vectorized to do one chunk of 256-bit -> 1ns but x64 only. Conveniently
    // GCC vector extensions can handle most of this.

    simd_vector uv;
    uv.v =
        (v & other.v & v_mask_disallowed_pos)
        |
        ((other.v & v_mask_count_min) < (v & v_mask_count_min))
        |
        ((v ^ v_mask_count_exact) < (other.v & v_mask_count_min))
        ;
    return _mm256_testz_si256(uv.y, uv.y);

    #elif USE_NEON
    // vectorized to do two chunks of 128-bit -> 1.5ns on ARM (graviton 2)
    
    const simd_vector& this_v(reinterpret_cast<const simd_vector&>(v));
    const simd_vector& other_v(reinterpret_cast<const simd_vector&>(other.v));

    simd_vector other_v_and_count_min;
    other_v_and_count_min.y0 = vandq_u8(other_v.y0, sv_mask_count_min.y0);
    other_v_and_count_min.y1 = vandq_u8(other_v.y1, sv_mask_count_min.y1);
    
    simd_vector a;
    simd_vector b;
    
    a.y0 = vandq_u8(vandq_u8(this_v.y0, other_v.y0), sv_mask_disallowed_pos.y0);
    a.y1 = vandq_u8(vandq_u8(this_v.y1, other_v.y1), sv_mask_disallowed_pos.y1);

    b.y0 = vcltq_u8(other_v_and_count_min.y0, vandq_u8(this_v.y0, sv_mask_count_min.y0));
    b.y1 = vcltq_u8(other_v_and_count_min.y1, vandq_u8(this_v.y1, sv_mask_count_min.y1));

    a.y0 = vorrq_u8(a.y0, b.y0);
    a.y1 = vorrq_u8(a.y1, b.y1);

    b.y0 = vcltq_u8(veorq_u8(this_v.y0, sv_mask_count_exact.y0), other_v_and_count_min.y0);
    b.y1 = vcltq_u8(veorq_u8(this_v.y1, sv_mask_count_exact.y1), other_v_and_count_min.y1);

    a.y0 = vorrq_u8(a.y0, b.y0);
    a.y1 = vorrq_u8(a.y1, b.y1);

    // or the two halves toegether, obviously don't do that if we move to 128-bit intrinsics.
    a.y0 = vorrq_u8(a.y0, a.y1);
    
    return !(a.yw0[0] | a.yw0[1]);
    
    #else
    // vectorized to do four chunks (3x64-bit and 1x16-bit = 208-bits/26-bytes total),
    // not using any special SIMD instructions -> 4ns
   
    if (w0 & other.w0 & w012_mask_disallowed_pos) return false;
    if (w1 & other.w1 & w012_mask_disallowed_pos) return false;
    if (w2 & other.w2 & w012_mask_disallowed_pos) return false;
    if (w3 & other.w3 &   w3_mask_disallowed_pos) return false;

    uint64_t other_w0_count_min = other.w0 & w012_mask_count_min;
    uint64_t other_w1_count_min = other.w1 & w012_mask_count_min;
    uint64_t other_w2_count_min = other.w2 & w012_mask_count_min;
    uint16_t other_w3_count_min = other.w3 &   w3_mask_count_min;

    if ((other_w0_count_min - (w0 & w012_mask_count_min)) & w012_mask_count_exact) return false;
    if ((other_w1_count_min - (w1 & w012_mask_count_min)) & w012_mask_count_exact) return false;
    if ((other_w2_count_min - (w2 & w012_mask_count_min)) & w012_mask_count_exact) return false;
    if ((other_w3_count_min - (w3 &   w3_mask_count_min)) &   w3_mask_count_exact) return false;

    uint64_t this_w0_count_min_flipped_exact = (w0 & (w012_mask_count_min|w012_mask_count_exact)) ^ w012_mask_count_exact;
    uint64_t this_w1_count_min_flipped_exact = (w1 & (w012_mask_count_min|w012_mask_count_exact)) ^ w012_mask_count_exact;
    uint64_t this_w2_count_min_flipped_exact = (w2 & (w012_mask_count_min|w012_mask_count_exact)) ^ w012_mask_count_exact;
    uint16_t this_w3_count_min_flipped_exact = (w3 & (  w3_mask_count_min|  w3_mask_count_exact)) ^   w3_mask_count_exact;
    
    if (((this_w0_count_min_flipped_exact >> 1) - (other_w0_count_min >> 1)) & w012_mask_count_exact) return false;
    if (((this_w1_count_min_flipped_exact >> 1) - (other_w1_count_min >> 1)) & w012_mask_count_exact) return false;
    if (((this_w2_count_min_flipped_exact >> 1) - (other_w2_count_min >> 1)) & w012_mask_count_exact) return false;
    if (((this_w3_count_min_flipped_exact >> 1) - (other_w3_count_min >> 1)) &   w3_mask_count_exact) return false;

    return true;

    #endif
    
    // old code for reference -> 9ns    
    /* 
    for (int zc = 0; zc < 26; zc++) {        
        if (c[zc].all & other.c[zc].all & 0x1F) {
          return false;
        } 
        if ((c[zc].all & 0x60) > (other.c[zc].all & 0x60)) {
            return false;
        }
        if ((c[zc].all ^ 0x80) < (other.c[zc].all & 0x60)) {
            return false;
        }
    }
    return true;
    */
}

void CMask::check_detail_reasons_exn(const Word& w) const {
    const CMask& other = w.cmask;

    std::stringstream problems;
    for (int zc = 0; zc < 26; zc++) {        
        if (c[zc].all & other.c[zc].all & 0x1F) {
            int bad_spots = c[zc].all & other.c[zc].all & 0x1F;
            for (int s = 0; s < 5; s++) {
                if (bad_spots & (1 << s)) {
                    problems << "You can't have letter " << (char)('A' + zc) << " in spot #" << (s+1) << ". ";
                }
            }
        } 
        if ((c[zc].all & 0x60) > (other.c[zc].all & 0x60)) {
            int count_need =       c[zc].count_min;
            problems << "You need " << count_need << " of letter " << (char)('A' + zc) << ". ";
        }
        if ((c[zc].all ^ 0x80) < (other.c[zc].all & 0x60)) {
            int count_has  = other.c[zc].count_min;
            int count_need =       c[zc].count_min;
            if (count_need == 0) {
                problems << "You know letter " << (char)('A' + zc) << " is absent. ";
            } else {
                problems << "You know there's exactly " << count_need << " of letter " << (char)('A' + zc) << " but you put in " << count_has << ". ";
            }
        }
    }
    std::string problems_str = problems.str();
    if (problems_str.empty()) return;
    throw std::runtime_error(problems_str);
}

CMask& CMask::apply(const CMask& m) {
#if USE_AVX    
    simd_vector temp1;
    simd_vector temp2;
    simd_vector temp3;

    temp1.v =   v & v_mask_count_min;
    temp2.v = m.v & v_mask_count_min;
    temp3.y = _mm256_max_epi8(temp1.y, temp2.y);
    v = temp3.v | ((v | m.v) & v_mask_exact_and_disallowed); 
#elif USE_NEON
    simd_vector temp1;
    simd_vector temp2;
    simd_vector temp3;

    simd_vector& tv(reinterpret_cast<simd_vector&>(v));
    const simd_vector& mv(reinterpret_cast<const simd_vector&>(m.v));

    temp1.y0 = vandq_u8(tv.y0, sv_mask_count_min.y0);
    temp1.y1 = vandq_u8(tv.y1, sv_mask_count_min.y1);
    temp2.y0 = vandq_u8(mv.y0, sv_mask_count_min.y0);
    temp2.y1 = vandq_u8(mv.y1, sv_mask_count_min.y1);
    temp3.y0 = vmaxq_u8(temp1.y0, temp2.y0);
    temp3.y1 = vmaxq_u8(temp1.y1, temp2.y1);

    temp1.y0 = vandq_u8(vorrq_u8(tv.y0, mv.y0), sv_mask_exact_and_disallowed.y0);
    temp1.y1 = vandq_u8(vorrq_u8(tv.y1, mv.y1), sv_mask_exact_and_disallowed.y1);
    
    tv.y0 = vorrq_u8(temp3.y0, temp1.y0);
    tv.y1 = vorrq_u8(temp3.y1, temp1.y1);
#else
    for (int i = 0; i < 26; i++) {
        c[i] = c[i].apply(m.c[i]);
    }
#endif
    return *this;
}

bool CMask::has_at_most_one_letter_undetermined() const {
    int total_pop_count =
        (__builtin_popcountll(w0 & w012_mask_disallowed_pos)) +
        (__builtin_popcountll(w1 & w012_mask_disallowed_pos)) +
        (__builtin_popcountll(w2 & w012_mask_disallowed_pos)) +
        (__builtin_popcount  (w3 &   w3_mask_disallowed_pos));
    return (total_pop_count >= 100);
}

bool CMask::has_at_least_one_green() const {
    int total_pop_count =
        (__builtin_popcountll(w0 & w012_mask_disallowed_pos)) +
        (__builtin_popcountll(w1 & w012_mask_disallowed_pos)) +
        (__builtin_popcountll(w2 & w012_mask_disallowed_pos)) +
        (__builtin_popcount  (w3 &   w3_mask_disallowed_pos));

    return (total_pop_count >= 25);
    // it might be possible to eliminate 25 things via yellows without getting any greens... but I doubt it?
}

int CMask::count_yellow_or_green(const bool* is_letter_valid) const {
    int yellows = 0;
    for (int zc = 0; zc < 26; zc++) {
        if (is_letter_valid == nullptr || is_letter_valid[zc]) {
            yellows+=c[zc].count_min;
        }
    }
    return yellows;
}

bool CMask::operator<(const CMask& m) const {
    if (w0 < m.w0) return true;
    if (w0 > m.w0) return false;
    if (w1 < m.w1) return true;
    if (w1 > m.w1) return false;
    if (w2 < m.w2) return true;
    if (w2 > m.w2) return false;
    if (w3 < m.w3) return true;
    if (w3 > m.w3) return false;
    return false;
}
bool CMask::operator==(const CMask& m) const {
    return (w0 == m.w0 &&
            w1 == m.w1 &&
            w2 == m.w2 &&
            w3 == m.w3);
}

size_t CMask::hash() const {
    return (w0 ^ w1 ^ w2 ^ w3);
}

SingleCharData CMask::get_single_char(char letter) const {
    return c[letter -'A'];
}

const unsigned int CMask::num_hex_chars = 52;

std::string CMask::to_hex() const {
    std::stringstream ss;
    ss << std::hex;
    for (unsigned int i = 0; i < num_hex_chars/2; i++) {
        ss << std::setw(2) << std::setfill('0') << (unsigned int)(*(reinterpret_cast<const unsigned char*>(this) + i));
    }
    return ss.str();
}

unsigned char cmask_char_to_hex(unsigned char c) {
    if (c >= '0' && c <= '9') return (c - '0');
    if (c >= 'A' && c <= 'F') return (c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (c - 'a' + 10);
    throw std::runtime_error("got non [0-9A-Fa-f] char in hex mask");
}

CMask CMask::of_hex(const std::string& s) {
    if (s.length() != num_hex_chars) throw std::runtime_error("hex mask should match num_hex_chars");

    CMask m;
    for (unsigned int i = 0; i < num_hex_chars / 2; i++) {
        unsigned char& dest = (*(reinterpret_cast<unsigned char*>(&m) + i));
        unsigned char c0 = cmask_char_to_hex(s[i*2 + 0]);
        unsigned char c1 = cmask_char_to_hex(s[i*2 + 1]);
        dest = (c0<<4) + c1;
    }

    return m;
}


const std::string const_esc_green   = "\033[30;42m";
const std::string const_esc_yellow  = "\033[30;43m";
const std::string const_esc_black   = "\033[37;40m";
const std::string const_esc_normal  = "\033[0m";

const std::string test_esc_green   = "#";
const std::string test_esc_yellow  = "~";
const std::string test_esc_black   = "";
const std::string test_esc_normal  = "";

void CMask::output(std::ostream& os, bool ansi_escapes) const {
    const std::string& esc_green  = (ansi_escapes ? const_esc_green  : test_esc_green);
    const std::string& esc_yellow = (ansi_escapes ? const_esc_yellow : test_esc_yellow);
    const std::string& esc_black  = (ansi_escapes ? const_esc_black  : test_esc_black);
    const std::string& esc_normal = (ansi_escapes ? const_esc_normal : test_esc_normal);

    int disallowed_by_pos[] = {0,0,0,0,0};
    int counts[26];
    bool any_exact = false;
    bool exact[26];
    for (int zc = 0 ; zc < 26 ; zc++) {
        counts[zc] = c[zc].get_count_min();
        exact[zc] = c[zc].get_count_exact();
        any_exact |= exact[zc];
        for (int i = 0 ; i < 5; i++) {
            if (c[zc].get_disallowed_pos() & (1 << i)) {
                disallowed_by_pos[i] |= (1 << zc);
            }
        }
    }
    for (int i = 0; i < 5 ; i++) {
        if (__builtin_popcount(disallowed_by_pos[i]) == 25) {
            os << esc_green << 
                (char)(('A' + __builtin_ffs(~disallowed_by_pos[i]) - 1));
        } else if (disallowed_by_pos[i] == 0) {
            os << esc_normal << "_";
        } else {
            for (int zc = 0; zc < 26; zc++) {
                if (disallowed_by_pos[i] & (1 << zc)) {
                    os << esc_black << (char)('A' + zc);
                }
            }        
        }
        os << esc_normal << " ";
    }

    os << esc_yellow;
    for (int zc = 0; zc < 26; zc++) {
        os << std::string(counts[zc], (char)('A' + zc));
    }        

    if (!any_exact) {
        os << esc_normal;
        return;
    }
    
    os << esc_black << " ";

    for (int zc = 0; zc < 26; zc++) {
        if (exact[zc]) {
            os << (char)('A' + zc);
        }
    }
    os << esc_normal;
}    

static bool use_ansi_formatting = true;

std::ostream& operator<<(std::ostream& os, const CMask& m) {
    m.output(os, use_ansi_formatting);
    return os;
}

void test1() {
    std::stringstream output;
    std::stringstream expected;

    use_ansi_formatting = false;
    
    output << sizeof(SingleCharData) << " " << sizeof(CMask) << std::endl;
    output << CMask("ABCDE") << std::endl;
    output << CMask("MAMMA") << std::endl;

    //expected << "1 26" << std::endl;
    expected << "1 32" << std::endl;
    expected << "A B C D E ~ABCDE" << std::endl;
    expected << "M A M M A ~AAMMM" << std::endl;

    // 1+ yellow
    output << CMask(Word("CRATE"), Word("STEAK")) << std::endl;
    expected << "_ T E A _ ~AET KS" << std::endl;

    // 1X yellow
    output << CMask(Word("CRATE"), Word("STEEK")) << std::endl;
    expected << "_ T E E _ ~ET EKS" << std::endl;

    // 2+ yellow
    output << CMask(Word("MUMMY"), Word("ZMZZM")) << std::endl;
    expected << "_ M _ _ M ~MM Z" << std::endl;

    // 2X yellow
    output << CMask(Word("GRAMM"), Word("MMMBC")) << std::endl;
    expected << "M M M _ _ ~MM BCM" << std::endl;
    
    // G / black
    output << CMask(Word("TRACK"), Word("ZCZCK")) << std::endl;
    expected << "_ _ _ #C #K ~CK CZ" << std::endl;

    // G / 1+ yellow
    output << CMask(Word("BAAAB"), Word("AACCC")) << std::endl;
    expected << "A #A _ _ _ ~AA C" << std::endl;

    // G / 1X yellow
    output << CMask(Word("BAACC"), Word("BBAAA")) << std::endl;
    expected << "#B _ #A A A ~AAB AB" << std::endl;

    // G / 2+ yellow
    output << CMask(Word("ZQQZZ"), Word("ZZZRR")) << std::endl;
    expected << "#Z Z Z _ _ ~ZZZ R" << std::endl;

    // 2G
    output << CMask(Word("AOAOZ"), Word("AEAEB")) << std::endl;
    expected << "#A _ #A _ _ ~AA BE" << std::endl;

    // 2G+B
    output << CMask(Word("AOAOZ"), Word("AEAEA")) << std::endl;
    expected << "#A _ #A _ _ ~AA AE" << std::endl;
    
    // 2+G
    output << CMask(Word("AOAOA"), Word("AEAEB")) << std::endl;
    expected << "#A _ #A _ _ ~AA BE" << std::endl;

    // 2G + Y
    output << CMask(Word("AOAOA"), Word("AAAZZ")) << std::endl;
    expected << "#A A #A _ _ ~AAA Z" << std::endl;

    // check [apply]
    output << CMask(Word("ABCDE"), Word("FGHIJ")).apply(CMask(Word("ABCDE"), Word("KLMNO"))) << std::endl;
    expected << "_ _ _ _ _ ~ FGHIJKLMNO" << std::endl;

    output << CMask(Word("ABCDE"), Word("AOOPP")).apply(CMask(Word("ABCDE"), Word("AODWE"))) << std::endl;
    expected << "#A _ D _ #E ~ADE OPW" << std::endl;

    output << CMask(Word("AABCD"), Word("AZZZB")).apply(CMask(Word("AABCD"), Word("AZABB"))) << std::endl;
    expected << "#A _ A B B ~AAB BZ" << std::endl;

    use_ansi_formatting = true;    

    std::string output_str = output.str();
    std::string expected_str = expected.str();
    if (output_str != expected_str) {
        throw std::runtime_error("Mask::test1()  failed, got\n" + output_str + ", but expected\n" + expected_str);
    }
}

void test2() {
    std::stringstream output;
    std::stringstream expected;

    CMask m;
    // 1+ yellow
    m = CMask(Word("CRATE"), Word("STEAK")); // black S___K, yellow _TEA_
    output << m.check(Word("CRATE"));
    output << m.check(Word("SRATE")); // uses black S
    output << m.check(Word("KRATE")); // uses black K
    output << m.check(Word("CEATZ")); // ok
    output << m.check(Word("CATEZ")); // ok
    output << m.check(Word("CETAZ")); // puts A in yellow
    output << m.check(Word("CAETZ")); // puts E in yellow
    output << m.check(Word("CTAEZ")); // puts T in yellow
    output << m.check(Word("CWTEZ")); // doesn't use A
    output << m.check(Word("CAWEZ")); // doesn't use T
    output << m.check(Word("CATWZ")); // doesn't use E
    output << m.check(Word("ERATE")); // ok
    output << m.check(Word("ARATE")); // ok
    output << m.check(Word("TRATE")); // ok
    output << std::endl;
    expected << true;
    expected << false;
    expected << false;
    expected << true;
    expected << true;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << true;
    expected << true;
    expected << true;
    expected << std::endl;


    // 1X yellow
    m = CMask(Word("CRATE"), Word("STEEK")); //black S__EK, yellow _TE__
    output << m.check(Word("CRATE"));
    output << m.check(Word("SRATE")); // uses black S
    output << m.check(Word("KRATE")); // uses black K
    output << m.check(Word("CEATZ")); // ok
    output << m.check(Word("CATEZ")); // puts E in yellow
    output << m.check(Word("CETAZ")); // ok
    output << m.check(Word("CAETZ")); // puts E in yellow
    output << m.check(Word("CTAEZ")); // puts T in yellow
    output << m.check(Word("CWTEZ")); // doesn't use A
    output << m.check(Word("CAWEZ")); // doesn't use T
    output << m.check(Word("CATWZ")); // doesn't use E
    output << m.check(Word("ERATE")); // uses two Es
    output << m.check(Word("ARATE")); // ok
    output << m.check(Word("TRATE")); // ok
    output << std::endl;
    expected << true;
    expected << false;
    expected << false;
    expected << true;
    expected << false;
    expected << true;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << true;
    expected << true;
    expected << std::endl;

    m = CMask(Word("MUMMY"), Word("ZMZZM"));
    output << m.check(Word("MUMMY"));
    output << m.check(Word("MZMMY")); // uses black Z
    output << m.check(Word("MQQQM")); // puts M in yellow
    output << m.check(Word("MUUUU")); // only one M
    output << std::endl;
    expected << true;
    expected << false;
    expected << false;
    expected << false;
    expected << std::endl;

    // 2X yellow
    m = CMask(Word("WHEEL"), Word("EERIE")); // black __RIE, yellow EE___
    output << m.check(Word("WHEEL"));
    output << m.check(Word("REEMM")); // uses black R
    output << m.check(Word("ZEEZZ")); // puts E in yellow
    output << m.check(Word("EZEZZ")); // puts E in yellow
    output << m.check(Word("ZEZZE")); // puts E in yellow
    output << m.check(Word("ZZEEZ")); // ok
    output << std::endl;
    expected << true;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << true;
    expected << std::endl;


    // G / 1+ yellow
    m = CMask(Word("BAAAB"), Word("AACCC")); // green _A____, yellow A____, black __CCC
    output << m.check(Word("BAAAB"));
    output << m.check(Word("CAAAB")); // uses black C
    output << m.check(Word("ZZAAB")); // ignores green A
    output << m.check(Word("ZZABB")); // ignores green A
    output << m.check(Word("AABBB")); // puts A in yellow
    output << m.check(Word("BABBB")); // doesn't use yellow A
    output << m.check(Word("BAABB")); // ok
    output << m.check(Word("BAABA")); // ok
    output << std::endl;
    expected << true;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << true;
    expected << true;
    expected << std::endl;

    // G / 1X yellow
    m = CMask(Word("BAACC"), Word("BBAAA")); // green B_A__, yellow ___A_, black ____A
    output << m.check(Word("BAACC"));
    output << m.check(Word("ZAACC")); // ignores green B
    output << m.check(Word("ZAABB")); // ignores green B
    output << m.check(Word("BAZCC")); // ignores green A
    output << m.check(Word("BAZAA")); // ignores green A
    output << m.check(Word("BAAQQ")); // ok
    output << m.check(Word("BQAAA")); // puts A in yellow, too many As
    output << m.check(Word("BAAQA")); // too many As
    output << std::endl;
    expected << true;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << true;
    expected << false;
    expected << false;
    expected << std::endl;
   
    m = CMask(Word("ZQQZZ"), Word("ZZZRR")); // green Z____, yellow _ZZ__, black Rs
    output << m.check(Word("ZQQZZ"));
    output << m.check(Word("ZRQZZ")); // uses black R
    output << m.check(Word("ZQZZQ")); // ignores yellow Z
    output << m.check(Word("ZQZZQ")); // ignores yellow Z
    output << m.check(Word("ZZQQZ")); // ignores yellow Z
    output << m.check(Word("ZQQZL")); // not enough Zs
    output << m.check(Word("ZQQLZ")); // not enough Zs
    output << m.check(Word("ZLLZZ")); // ok
    output << std::endl;
    expected << true;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << false;
    expected << true;
    expected << std::endl;

    // 2G    
    m = CMask(Word("AOAOZ"), Word("AEAEB")); // green A_A__, rest black
    output << m.check(Word("AOAOZ")); // ok
    output << m.check(Word("AOAOA")); // pl
    output << m.check(Word("AOAOE")); // uses black E
    output << std::endl;
    expected << true;
    expected << true;
    expected << false;
    expected << std::endl;

    // 2+G
    m = CMask(Word("AOAOA"), Word("AEAEB")); // green A_A__, rest black
    output << m.check(Word("AOAOA")); // ok
    output << m.check(Word("AOAOZ")); // ok
    output << m.check(Word("AOAOE")); // uses black E
    output << std::endl;
    expected << true;
    expected << true;
    expected << false;
    expected << std::endl;

    // 2G + Y
    m = CMask(Word("AOAOA"), Word("AAAZZ"));
    output << m.check(Word("AOAOA")); // ok
    output << m.check(Word("AAAQQ")); // puts middle A in yellow
    output << m.check(Word("AOAAO")); // ok
    output << std::endl;
    expected << true;
    expected << false;
    expected << true;
    expected << std::endl;

    use_ansi_formatting = true;    

    std::string output_str = output.str();
    std::string expected_str = expected.str();
    if (output_str != expected_str) {
        throw std::runtime_error("CMask::test2()  failed, got\n" + output_str + ", but expected\n" + expected_str);
    }
}

void test3() {
    std::stringstream output;
    std::stringstream expected;
    CMask m;
    
    m = CMask(Word("ABCDE"), Word("FGHIJ"));
    output   << m.has_at_most_one_letter_undetermined() << " " << m.has_at_least_one_green() << " " << m.count_yellow_or_green(nullptr) << std::endl;
    expected << "0 0 0" << std::endl;

    m = CMask(Word("ABCDE"), Word("EEAAA"));
    output   << m.has_at_most_one_letter_undetermined() << " " << m.has_at_least_one_green() << " " << m.count_yellow_or_green(nullptr) << std::endl;
    expected << "0 0 2" << std::endl;

    m = CMask(Word("ABCDE"), Word("AZZZC"));
    output   << m.has_at_most_one_letter_undetermined() << " " << m.has_at_least_one_green() << " " << m.count_yellow_or_green(nullptr) << std::endl;
    expected << "0 1 2" << std::endl;

    m = CMask(Word("FFGGH"), Word("GGFFZ"));
    output   << m.has_at_most_one_letter_undetermined() << " " << m.has_at_least_one_green() << " " << m.count_yellow_or_green(nullptr) << std::endl;
    expected << "0 0 4" << std::endl;

    m = CMask(Word("FFGGH"), Word("GGFFH"));
    output   << m.has_at_most_one_letter_undetermined() << " " << m.has_at_least_one_green() << " " << m.count_yellow_or_green(nullptr) << std::endl;
    expected << "0 1 5" << std::endl;

    m = CMask(Word("ABCDE"), Word("ZBCDE"));
    output   << m.has_at_most_one_letter_undetermined() << " " << m.has_at_least_one_green() << " " << m.count_yellow_or_green(nullptr) << std::endl;
    expected << "1 1 4" << std::endl;

    m = CMask(Word("ABCDE"), Word("ABCDZ"));
    output   << m.has_at_most_one_letter_undetermined() << " " << m.has_at_least_one_green() << " " << m.count_yellow_or_green(nullptr) << std::endl;
    expected << "1 1 4" << std::endl;

    output << (*(Dictionary::to_word_index(Word("ABACK")))) << std::endl;
    output << (*(Dictionary::to_word_index(Word("ZONAL")))) << std::endl;
    output << (*(Dictionary::to_word_index(Word("AAHED")))) << std::endl;
    output << (*(Dictionary::to_word_index(Word("ZYMIC")))) << std::endl;

    expected << "ABACK" << std::endl;
    expected << "ZONAL" << std::endl;
    expected << "AAHED" << std::endl;
    expected << "ZYMIC" << std::endl;              
    
    std::string output_str = output.str();
    std::string expected_str = expected.str();
    if (output_str != expected_str) {
        throw std::runtime_error("CMask::test3()  failed, got\n" + output_str + ", but expected\n" + expected_str);
    }
}

using std::vector;
using boost::posix_time::ptime;
using boost::posix_time::microsec_clock;

void test4() {    
    const std::vector<Dictionary::WordIndex>& answers = Dictionary::get_all_answers();
    const std::vector<Dictionary::WordIndex>& guesses = Dictionary::get_all_answers_and_guesses();

    int r;
    int n;
    ptime start;
    
    r = 0;
    n = 0;
    start = microsec_clock::local_time();
    for (size_t a = 0 ; a < answers.size() - 100; a+=15) {
        for (size_t g = 0 ; g < guesses.size(); g+=15) {
            CMask m1(*answers[a], *guesses[g]);
            CMask m2(Result(*answers[a], *guesses[g]));
            if (!(m1 == m2)) throw std::runtime_error("CMask::test4 fail0");
            r++;
        }
    }
    int micros0 = (microsec_clock::local_time() - start).total_microseconds();

    r = 0;
    n = 0;
    start = microsec_clock::local_time();
    for (size_t a = 0 ; a < answers.size() - 100; a+=15) {
        for (size_t g = 0 ; g < guesses.size(); g+=15) {
            CMask m(*answers[a], *guesses[g]);
            if (m.check(*answers[a])) n++;

            r++;
        }
    }
    int micros1 = (microsec_clock::local_time() - start).total_microseconds();
    if (n != r) throw std::runtime_error("CMask::test4 fail1");

    r = 0;
    n = 0;
    start = microsec_clock::local_time();        
    for (size_t a = 0 ; a < answers.size() - 100; a+=15) {
        for (size_t g = 0 ; g < guesses.size(); g+=15) {
            CMask m(*answers[a], *guesses[g]);

            if (m.check(*answers[a])) n++;
            if (m.check(*answers[a+1])) n++;
            if (m.check(*answers[a+2])) n++;
            if (m.check(*answers[a+3])) n++;
            if (m.check(*answers[a+4])) n++;
            if (m.check(*answers[a+5])) n++;
            if (m.check(*answers[a+6])) n++;
            if (m.check(*answers[a+7])) n++;

            r++;
        }
    }
    int micros2 = (microsec_clock::local_time() - start).total_microseconds();
    if (n != 348637) throw std::runtime_error("CMask::test4 fail2"); // dumb checksum

    int micros_total = micros1 + micros2;
    int micros_check = (micros2 - micros1) / 7;
    int micros_create = (micros1 * 8 - micros2) / 7;

    // suppress the warnings:
    micros0++;
    micros_total++;
    micros_check++;
    micros_create++;
    //std::cerr << "Mask consistency check took " << micros0/1e6 << "s" << std::endl;
    //std::cerr << "Mask tests took " << micros_total/1e6 << "s total, creating was " << micros_create*1000.0/n << "ns each and checking was " << micros_check*1000.0/n << "ns each." << std::endl;
    // typical: Mask tests took 0.047197s total, creating was 25.4448ns each and checking was 9.38512ns each.
}

void CMask::test() {
    test1();
    test2();
    test3();
    test4();
}
