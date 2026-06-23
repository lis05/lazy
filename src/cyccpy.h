#pragma once

// Originally made for me by https://github.com/welcome-to-the-sunny-side :3

#include <immintrin.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cyccpy {
static inline void cyccpy1(uint8_t* src, uint32_t dis, uint32_t n) {
    uint32_t i = 0;
    while (i < n) {
        src[i + dis] = src[i];
        i++;
    }
}

static inline void cyccpy4(uint8_t* src, uint32_t dis, uint32_t n) {
    uint32_t i = 4;
    *(reinterpret_cast<uint32_t*>(src + dis)) = *(reinterpret_cast<uint32_t*>(src));
    while (i < n) [[unlikely]] {
        *(reinterpret_cast<uint32_t*>(src + i + dis)) =
            *(reinterpret_cast<uint32_t*>(src + i));
        i += 4;
    }
}
static inline void cyccpy8(uint8_t* src, uint32_t dis, uint32_t n) {
    uint32_t i = 8;
    *(reinterpret_cast<uint64_t*>(src + dis)) = *(reinterpret_cast<uint64_t*>(src));
    while (i < n) [[unlikely]] {
        *(reinterpret_cast<uint64_t*>(src + i + dis)) =
            *(reinterpret_cast<uint64_t*>(src + i));
        i += 8;
    }
}

static inline void cyccpy16(uint8_t* src, uint32_t dis, uint32_t n) {
    uint32_t i = 16;
    __m128i  reg = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(src + dis), reg);
    while (i < n) [[unlikely]] {
        __m128i reg = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(src + (i + dis)), reg);
        i += 16;
    }
}

static inline void cyccpy32(uint8_t* src, uint32_t dis, uint32_t n) {
    uint32_t i = 32;
    __m256i  reg = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(src + dis), reg);
    while (i < n) [[unlikely]] {
        __m256i reg = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(src + (i + dis)), reg);
        i += 32;
    }
}

static inline void cyccpy64(uint8_t* src, uint32_t dis, uint32_t n) {
    uint32_t i = 64;
    __m512i  reg = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(src));
    _mm512_storeu_si512(reinterpret_cast<__m512i*>(src + dis), reg);
    while (i < n) [[unlikely]] {
        __m512i reg = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(src + i));
        _mm512_storeu_si512(reinterpret_cast<__m512i*>(src + (i + dis)), reg);
        i += 64;
    }
}

static inline void cyccpy(uint8_t* src, uint32_t dis, uint32_t n) {
    switch (dis) {
    case 0:
    case 1:
    case 2:
    case 3:
        cyccpy1(src, dis, n);
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        cyccpy4(src, dis, n);
        break;

    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        cyccpy8(src, dis, n);
        break;
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
        cyccpy16(src, dis, n);
        break;
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
    case 61:
    case 62:
    case 63:
        cyccpy32(src, dis, n);
        break;
    default:
        cyccpy64(src, dis, n);
        break;
    }
}
}  // namespace cyccpy
