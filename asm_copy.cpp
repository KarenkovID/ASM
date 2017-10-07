#include <iostream>
#include <emmintrin.h>

using namespace std;

const size_t BLOCK_SIZE = 16;

void copyUsingAsm(char const *src, char *dst, size_t size) {
    size_t i = 0;

    for (; i < BLOCK_SIZE && (size_t)(dst + i) % BLOCK_SIZE != 0; ++i) {
        *dst = *src;
        src++;
        dst++;
    }

    ssize_t back = (size - i) % BLOCK_SIZE;

    for (size_t j = i; j < size - back; j += BLOCK_SIZE) {
        __m128i reg;

        __asm__ volatile ("movdqu\t (%1), %0\n"
                "movntdq\t %0, (%2)\n"
        : "=x"(reg)
        : "r"(src), "r"(dst)
        :"memory");

        src += BLOCK_SIZE;
        dst += BLOCK_SIZE;
    }

    for (size_t k = (size - back); k < size; ++k) {
        *dst = *src;
        ++dst;
        ++src;
    }

    _mm_sfence();
}
void copyUsingAsm(void const *src, void *dst, size_t size) {
    copyUsingAsm(static_cast<char const*>(src), static_cast<char*>(dst), size);
}

const int TESTS_COUNT = 1000;
const int MAX_SIZE = 10000;
int main() {
    srand((unsigned int) time(NULL));

#pragma omp parallel for
    for (int i = 0; i < TESTS_COUNT; ++i) {
        int size = rand() % MAX_SIZE + 1;
        int src[size];
        int dst[size];

        for (size_t j = 0; j < size; ++j) {
            src[j] = rand();
        }

        copyUsingAsm(src, dst, sizeof(src));

        //checking
        for (int j = 0; j < size; ++j) {
            if (src[j] != dst[j]) {
                cout << "bad trip";
            }
        }
    }
    cout << "good trip";
    return 0;
}