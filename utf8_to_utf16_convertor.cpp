#include <stdint.h>
#include <string>
#include <xmmintrin.h>
#include <tmmintrin.h>
#include <vector>
#include <bitset>
#include <iostream>

using namespace std;

const int BLOCK_SIZE = 16;

/**
 *
 * @param src
 * @param dst
 * @return number of bytes, read and converted from src
 */
size_t utf8_to_utf16_one(unsigned char *&src, unsigned char *&dst) {
    unsigned long unicode;
    size_t i = 0;
    size_t octetsReaded;
    {
        unsigned long uni;
        size_t todo;
        unsigned char ch = src[i++];
        if (ch <= 0x7F) {
            uni = ch;
            todo = 0;
        } else if (ch <= 0xBF) {
            throw logic_error("not a UTF-8 string");
        } else if (ch <= 0xDF) {
            uni = ch & 0x1F;
            todo = 1;
        } else if (ch <= 0xEF) {
            uni = ch & 0x0F;
            todo = 2;
        } else if (ch <= 0xF7) {
            uni = ch & 0x07;
            todo = 3;
        } else {
            throw logic_error("not a UTF-8 string");
        }
        octetsReaded = todo + 1;
        for (size_t j = 0; j < todo; ++j) {
//            if (i == 1)
//                throw logic_error("not a UTF-8 string");
            unsigned char ch = src[i++];
            if (ch < 0x80 || ch > 0xBF)
                throw logic_error("not a UTF-8 string");
            uni <<= 6;
            uni += ch & 0x3F;
        }
        if (uni >= 0xD800 && uni <= 0xDFFF)
            throw logic_error("not a UTF-8 string");
        if (uni > 0x10FFFF)
            throw logic_error("not a UTF-8 string");
        unicode = uni;
    }
    src += octetsReaded;
    size_t octetsWrited;
    std::wstring utf16;
    {
        unsigned long uni = unicode;
        if (uni <= 0xFFFF) {
            dst[0] = (unsigned char) (uni >> 8);
            dst[1] = (unsigned char) uni;
            octetsWrited = 2;
        } else {
            uni -= 0x10000;
            auto firstHalf = (unsigned short)((uni >> 10) + 0xD800);
            auto secondHalf = (unsigned short)((uni & 0x3FF) + 0xDC00);
            dst[0] = (unsigned char)(firstHalf >> 8);
            dst[1] = (unsigned char) firstHalf;
            dst[2] = (unsigned char) (secondHalf >> 8);
            dst[3] = (unsigned char) (secondHalf);
            octetsWrited = 4;
        }
    }
    dst += octetsWrited;
    return octetsReaded;
}

const __m128i ZEROS = _mm_setzero_si128();

size_t utf8_to_utf16_combined(const unsigned char *srcImmut, const unsigned char *dstImmut, int len) {
    auto *src = const_cast<unsigned char *>(srcImmut);
    auto *dst = const_cast<unsigned char *>(dstImmut);
    wstring utf16 = wstring();
    while (len >= 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src));

        //is first bites in bytes is zero (ASCI equivalent)
        //calculate the most significant bit for each byte and store it in int
        if (!_mm_movemask_epi8(chunk)) {
            //little-endian order for vectors of data
            __m128i firstHalf = _mm_unpacklo_epi8(ZEROS, chunk);
            __m128i secondHalf = _mm_unpackhi_epi8(ZEROS, chunk);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(dst), firstHalf);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(dst + 16), secondHalf);
            dst += 32;
            src += 16;
            len -= 16;
            continue;
        } else {
            len -= utf8_to_utf16_one(src, dst);
        }
    }
    while (len > 0) {
        len -= utf8_to_utf16_one(src, dst);
    }
    return dst - dstImmut;
}

size_t utf8_to_utf16_scalar(const unsigned char *srcImmut, const unsigned char *dstImmut, int len) {
    auto *src = const_cast<unsigned char *>(srcImmut);
    auto *dst = const_cast<unsigned char *>(dstImmut);
    while (len > 0) {
        len -= utf8_to_utf16_one(src, dst);
    }
    return dst - dstImmut;
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
    auto *str = new unsigned char[100];
    for (int i = 0; i < 100; ++i) {
        int mod = i % 16;
        switch (mod) {
            case 0:
                str[i] = 'A';
                break;
            case 1:
                str[i] = 'B';
                break;
            case 2:
                str[i] = 'C';
                break;
            case 3:
                str[i] = 'D';
                break;
            case 4:
                str[i] = 'E';
                break;
            case 5:
                str[i] = 'F';
                break;
            case 6:
                str[i] = 'G';
                break;
            case 7:
                str[i] = 'H';
                break;
            case 8:
                str[i] = 'I';
                break;
            case 9:
                str[i] = 'J';
                break;
            case 10:
                str[i] = 'K';
                break;
            case 11:
                str[i] = 'L';
                break;
            case 12:
                str[i] = 'M';
                break;
            case 13:
                str[i] = 'N';
                break;
            case 14:
                str[i] = 'O';
                break;
            case 15:
                str[i] = 'P';
                break;
        }
        mod = i % 4;
        switch (mod) {
            case 0:
                str[i] = 0b11110000;
                break;
            case 1:
                str[i] = 0b10010000;
                break;
            default:
                str[i] = 0b10000000;
        }
//        if (i % 2 == 0) {
//            str[i] = 0b11010001;
//        } else {
//            str[i] = 0b10001111;
//        }
//        str[i] = 'A';
    }

    for (int i = 0; i < 100; i += 1) {
        cout << std::hex << (int) str[i] << " ";
    }
    cout << endl;
    auto *dstSIMD = new unsigned char[400];
    auto *dstScalar = new unsigned char[400];
    size_t outLengthSIMD = utf8_to_utf16_combined(str, dstSIMD, 100);
    size_t outLengthScalar = utf8_to_utf16_scalar(str, dstScalar, 100);

    if (outLengthScalar != outLengthSIMD) {
        cout << "Wrong size. Expected size is " << outLengthScalar << ". Out size is " << outLengthSIMD;
        return 0;
    }

    cout << "Output lingth is " << outLengthScalar << endl;

    cout << endl;
    for (size_t j = 0; j < outLengthSIMD; j++) {
        cout << std::hex << (int) dstSIMD[j] << " ";
    }
    cout << endl;
    for (size_t j = 0; j < outLengthScalar; j++) {
        cout << std::hex << (int) dstScalar[j] << " ";
    }

    for (size_t j = 0; j < outLengthScalar; j++) {
        if (dstScalar[j] != dstSIMD[j]) {
            cout << "WA";
            return 0;
        }
    }


    return 0;
}