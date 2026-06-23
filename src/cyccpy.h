#pragma once

// Originally made for me by https://github.com/welcome-to-the-sunny-side :3

#include <immintrin.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cyccpy {
static inline int cnt1 = 0;
static inline void cyccpy1(uint8_t* src, uint32_t dis, uint32_t n) {
    cnt1++;
    uint32_t i = 0;
    while (i < n) {
        src[i + dis] = src[i];
        i++;
    }
}

static inline int cnt4 = 0;
static inline void cyccpy4(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 4) [[likely]] {
        cnt4++;
        uint32_t i = 4;
        *(reinterpret_cast<uint32_t*>(src + dis)) =
            *(reinterpret_cast<uint32_t*>(src));
        while (i < n) [[unlikely]] {
            *(reinterpret_cast<uint32_t*>(src + i + dis)) =
                *(reinterpret_cast<uint32_t*>(src + i));
            i += 4;
        }
    } else {
        cyccpy1(src, dis, n);
    }
}

static inline int cnt8 = 0;
static inline void cyccpy8(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 8) [[likely]] {
        cnt8++;
        uint32_t i = 8;
        *(reinterpret_cast<uint64_t*>(src + dis)) =
            *(reinterpret_cast<uint64_t*>(src));
        while (i < n) [[unlikely]] {
            *(reinterpret_cast<uint64_t*>(src + i + dis)) =
                *(reinterpret_cast<uint64_t*>(src + i));
            i += 8;
        }
    } else {
        cyccpy4(src, dis, n);
    }
}

static inline int cnt16  = 0;
static inline void cyccpy16(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 16) [[likely]] {
        cnt16++;
        uint32_t i = 16;
        __m128i  reg = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(src + dis), reg);
        while (i < n) [[unlikely]] {
            __m128i reg = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(src + (i + dis)), reg);
            i += 16;
        }
    } else {
        cyccpy8(src, dis, n);
    }
}

static inline int cnt32 = 0;
static inline void cyccpy32(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 32) [[likely]] {
        cnt32++;
        uint32_t i = 32;
        __m256i  reg = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(src + dis), reg);
        while (i < n) [[unlikely]] {
            __m256i reg =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(src + (i + dis)), reg);
            i += 32;
        }
    } else {
        cyccpy16(src, dis, n);
    }
}

static inline int cnt64 = 0;
static inline void cyccpy64(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 64) [[likely]] {
        cnt64++;
        uint32_t i = 64;
        __m512i  reg = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(src));
        _mm512_storeu_si512(reinterpret_cast<__m512i*>(src + dis), reg);
        while (i < n) [[unlikely]] {
            // TODO: this copies too many bytes i think... len?
            __m512i reg =
                _mm512_loadu_si512(reinterpret_cast<const __m512i*>(src + i));
            _mm512_storeu_si512(reinterpret_cast<__m512i*>(src + (i + dis)), reg);
            i += 64;
        }
    } else {
        cyccpy32(src, dis, n);
    }
}

static inline void cyccpy(uint8_t* src, uint32_t dis, uint32_t n) {
    cyccpy32(src, dis, n);
}
}  // namespace cyccpy
