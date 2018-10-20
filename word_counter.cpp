#include <stdint.h>
#include <xmmintrin.h>
#include <tmmintrin.h>
#include <string>
#include <iostream>

using namespace std;

const int BLOCK_SIZE = 16;
const bool testPerformance = true;

inline size_t greed(string s, size_t n) {
    size_t ans = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] != ' ' && (s[i - 1] == ' ' || i == 0)) {
            ++ans;
        }
    }
    return ans;
}

inline void flush(size_t &res, __m128i &a) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        res += (int) *((uint8_t *) &a + i);
    }
    a = _mm_setzero_si128();
}

size_t count(const char *str, size_t size) {
    static const __m128i MASK_SPACES = _mm_set_epi8(' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    static const __m128i MASK_ONES = _mm_set_epi8(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);

    size_t res = 0;
    size_t offset = 0;

    if (*str != ' ') {
        ++res;
    }

    //aligning
    bool was_space = false;
    for (; (size_t) (str + offset) % BLOCK_SIZE != 0; offset++) {
        bool isSpace = str[offset] == ' ';
        res += (was_space && !isSpace);
        was_space = isSpace;
    }

    if (was_space && offset != 0 && *(str + offset) != ' ') {
        ++res;
    }

    __m128i ans = _mm_setzero_si128();
    size_t n = size - (size - offset) % BLOCK_SIZE - BLOCK_SIZE;
    __m128i curSpaces = _mm_cmpeq_epi8(_mm_load_si128((__m128i *) (str + offset)), MASK_SPACES);
    __m128i nextSpaces;
    int k = 0;

    //assembly magic (no)
    for (; offset < n; offset += BLOCK_SIZE) {
        nextSpaces = _mm_cmpeq_epi8(_mm_load_si128((__m128i *) (str + offset + BLOCK_SIZE)), MASK_SPACES);
        //https://msdn.microsoft.com/en-us/library/bb514041(v=vs.120).aspx
        __m128i shiftedCurSpaces = _mm_alignr_epi8(nextSpaces, curSpaces, 1);
        __m128i count = _mm_and_si128(_mm_andnot_si128(shiftedCurSpaces, curSpaces), MASK_ONES);
        ans = _mm_add_epi8(ans, count);

        ++k;
        if (k == 255) {
            flush(res, ans);
            k = 0;
        }
        curSpaces = nextSpaces;
    }

    flush(res, ans);
    offset = n;
    if (*(str + offset - 1) == ' ' && *(str + offset) != ' ') {
        --res;
    }
    was_space = *(str + offset - 1) == ' ';
    for (size_t i = offset; i < size; i++) {
        char cur = *(str + i);
        res += (was_space && cur != ' ');
        was_space = (cur == ' ');
    }

    return res;
}

string test() {
    std::string s = "";
    int len = (rand() % 100000);
    for (int i = 0; i < len; ++i) {
        int k = rand() % 2;
        if (k) {
            s += "A";
        } else {
            s += " ";
        }
    }
    return s;
}

int main() {
    srand(time(0));
    for (int i = 0; i < 1000; i++) {
        std::string s = test();
        const char *st = s.c_str();
        cout << s << "\n";
        size_t right_ans = greed(s, (int) s.size());
        size_t my_ans = count(st, s.size());
        cout << "right: " << right_ans << "\n";
        cout << "my: " << my_ans << "\n";
        if (right_ans != my_ans) {
            cout << ":(\n";
            return 0;
        }
        cout << "OK\n";
    }
    return 0;
}