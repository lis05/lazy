#pragma once

// Originally made for me by https://github.com/welcome-to-the-sunny-side :3

#include <immintrin.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cyccpy {

static inline void memcpy1(uint8_t* dest, uint8_t* src) {
    *dest = *src;
}

static inline void memcpy8(uint8_t* dest, uint8_t* src) {
    std::memcpy(dest, src, 8);
}

static inline void memcpy16(uint8_t* dest, uint8_t* src) {
    __m128i reg = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dest), reg);
}

static inline void memcpy32(uint8_t* dest, uint8_t* src) {
    __m256i reg = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest), reg);
}

static inline void cyccpy(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 32) [[likely]] {
        uint32_t i = 32;
        memcpy32(src + dis, src);
        while (i < n) [[unlikely]] {
            i += 32;
            src += 32;
            memcpy32(src + dis, src);
        }
    } else if (dis >= 16) {
        uint32_t i = 16;
        memcpy16(src + dis, src);
        while (i < n) [[unlikely]] {
            i += 16;
            src += 16;
            memcpy16(src + dis, src);
        }
    } else if (dis >= 8) {
        uint32_t i = 8;
        memcpy8(src + dis, src);
        while (i < n) {
            i += 8;
            src += 8;
            memcpy8(src + dis, src);
        }
    } else {
        // n >= 5
        uint32_t i = 5;
        src[0 + dis] = src[0];
        src[1 + dis] = src[1];
        src[2 + dis] = src[2];
        src[3 + dis] = src[3];
        src[4 + dis] = src[4];
        while (i < n) {
            src[i + dis] = src[i];
            i++;
        }
    }
}
}  // namespace cyccpy
