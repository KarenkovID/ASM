#include <bits/stdc++.h>
#include <stdint.h>
#include <xmmintrin.h>
#include <tmmintrin.h>

using namespace std;

const int BLOCK_SIZE = 16;

size_t linearly(string s, size_t n) {
    size_t ans = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] != ' ' && (s[i - 1] == ' ' || i == 0)) {
            ++ans;
        }
    }
    return ans;
}

void recount_flush(size_t &res, __m128i &a) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        res += (int) *((uint8_t *) &a + i);
    }
    a = _mm_setzero_si128();
}

size_t count(const char *str, size_t size) {
    const __m128i MASK_SPACES = _mm_set_epi8(' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
                                             ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    const __m128i MASK_ONES = _mm_set_epi8(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
    size_t res = 0;
    size_t offset = 0;
    if (*str != ' ') {
        ++res;
    }

    //aligning
    bool was_space = false;
    for (; (size_t) (str + offset) % BLOCK_SIZE != 0; offset++) {
        res += (was_space && str[offset] != ' ');
        was_space = (str[offset] == ' ');
    }

    if (was_space && offset != 0 && *(str + offset) != ' ') {
        ++res;
    }

    __m128i ans = _mm_setzero_si128();
    size_t n = size - (size - offset) % BLOCK_SIZE - BLOCK_SIZE;
    __m128i prev, curr = _mm_cmpeq_epi8(_mm_load_si128((__m128i *) (str + offset)), MASK_SPACES);
    int k = 0;
    for (; offset < n; offset += BLOCK_SIZE) {
        prev = curr;
        curr = _mm_cmpeq_epi8(_mm_load_si128((__m128i *) (str + offset + BLOCK_SIZE)), MASK_SPACES);
        __m128i shifted_spaces = _mm_alignr_epi8(curr, prev, 1);
        __m128i count = _mm_and_si128(_mm_andnot_si128(shifted_spaces, prev), MASK_ONES);
        ans = _mm_add_epi8(ans, count);
        ++k;
        //protection from overflow
        if (k == 255) {
            recount_flush(res, ans);
            k = 0;
        }
    }

    recount_flush(res, ans);

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
    int len = (rand() % 100000g);
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
        size_t right_ans = linearly(s, (int) s.size());
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