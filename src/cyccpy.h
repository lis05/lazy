#pragma once

// Originally made for me by https://github.com/welcome-to-the-sunny-side :3

#include <immintrin.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cyccpy {
static void cyccpy16(uint8_t* src, uint32_t dis, uint32_t n);
static void cyccpy8(uint8_t* src, uint32_t dis, uint32_t n);

static void cyccpy32(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 32) [[likely]] {
        uint32_t i = 0;
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

static void cyccpy16(uint8_t* src, uint32_t dis, uint32_t n) {
    if (dis >= 16) [[likely]] {
        uint32_t i = 0;
        while (i < n) [[unlikely]] {
            __m128i reg = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(src + (i + dis)), reg);
            i += 16;
        }
    } else {
        cyccpy8(src, dis, n);
    }
}

static void cyccpy8(uint8_t* src, uint32_t dis, uint32_t n) {
    uint32_t i = 0;
    if (dis >= 8) [[likely]] {
        uint32_t i = 0;
        while (i < n) [[unlikely]] {
            *(reinterpret_cast<uint64_t*>(src + i + dis)) =
                *(reinterpret_cast<uint64_t*>(src + i));
            i += 8;
        }
    } else {
        while (i < n) {
            src[i + dis] = src[i];
            i++;
        }
    }
}
}  // namespace cyccpy
