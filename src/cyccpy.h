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
    *(reinterpret_cast<uint64_t*>(dest)) = *(reinterpret_cast<uint64_t*>(src));
}

static inline void memcpy16(uint8_t* dest, uint8_t* src) {
    __m128i reg = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dest), reg);
}

static inline void memcpy32(uint8_t* dest, uint8_t* src) {
    __m256i reg = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest), reg);
}

static inline int  cnt1 = 0;
static inline void cyccpy1(uint8_t* src, uint32_t dis, uint32_t n) {
    cnt1++;
    uint32_t i = 0;
    while (i < n) {
        src[i + dis] = src[i];
        i++;
    }
}

static inline int  cnt8 = 0;
static inline void cyccpy8(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 8) [[likely]] {
        cnt8++;
        uint32_t i = 8;
        memcpy8(src + dis, src);
        while (i < n) [[unlikely]] {
            i += 8;
            src += 8;
            memcpy8(src + dis, src);
        }
    } else {
        cyccpy1(src, dis, n);
    }
}

static inline int  cnt16 = 0;
static inline void cyccpy16(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 16) [[likely]] {
        cnt16++;
        uint32_t i = 16;
        memcpy16(src + dis, src);
        while (i < n) [[unlikely]] {
            i += 16;
            src += 16;
            memcpy16(src + dis, src);
        }
    } else {
        cyccpy8(src, dis, n);
    }
}

static inline int  cnt32 = 0;
static inline int shorter = 0, longer = 0;
static inline void cyccpy32(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 32) [[likely]] {
        cnt32++;
        shorter += n < 32;
        longer += n >= 32;
        uint32_t i = 32;
        memcpy32(src + dis, src);
        while (i < n) [[unlikely]] {
            i += 32;
            src += 32;
            memcpy32(src + dis, src);
        }
    } else {
        cyccpy16(src, dis, n);
    }
}

// TODO: limit len to 32

static inline void cyccpy(uint8_t* src, uint32_t dis, uint32_t n) {
    cyccpy32(src, dis, n);
}
}  // namespace cyccpy
