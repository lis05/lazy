#pragma once

#include <immintrin.h>

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace strmatch {
static __attribute__((always_inline)) inline uint32_t match_simd_loop(
    uint32_t N, const std::byte *left, const std::byte *right) {
    int i = 0;

    if (i + 7 < N) {
        uint64_t l, r;
        memcpy(&l, left + i, sizeof(uint64_t));
        memcpy(&r, right + i, sizeof(uint64_t));
        if (l != r) {
            return i + std::countr_zero(l ^ r) / 8;
        }
        i += 8;
    }

    // 1. AVX-512 + AVX-512BW (64-byte chunks with byte-level masking)
#if defined(__AVX512F__) && defined(__AVX512BW__)
    for (; i + 63 < N; i += 64) {
        __m512i l = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(left + i));
        __m512i r = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(right + i));

        auto mask = _mm512_cmpeq_epi8_mask(l, r);
        if (mask != ~0x0ULL) {
            return i + std::countr_zero(~mask);
        }
    }
#endif

    // 2. Plain AVX-512 (64-byte chunks, fallback to quadword testing)
#if defined(__AVX512F__) && !defined(__AVX512BW__)
    for (; i + 63 < N; i += 64) {
        __m512i l = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(left + i));
        __m512i r = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(right + i));

        __m512i  xor_res = _mm512_xor_si512(l, r);
        uint32_t mask = _mm512_test_epi64_mask(xor_res, xor_res);
        if (mask != 0) {
            int      qw_idx = std::countr_zero(mask);
            uint64_t l_qw, r_qw;
            memcpy(&l_qw, left + i + qw_idx * 8, sizeof(uint64_t));
            memcpy(&r_qw, right + i + qw_idx * 8, sizeof(uint64_t));
            return i + qw_idx * 8 + std::countr_zero(l_qw ^ r_qw) / 8;
        }
    }
#endif

    // 3. AVX2 + AVX-512VL + AVX-512BW (32-byte chunks with direct 32-bit masking)
#if defined(__AVX2__) && defined(__AVX512VL__) && defined(__AVX512BW__)
    for (; i + 31 < N; i += 32) {
        __m256i l = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(left + i));
        __m256i r = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(right + i));

        __mmask32 mask = _mm256_cmpeq_epi8_mask(l, r);
        if (mask != 0xFFFFFFFF) {
            return i + std::countr_zero(~mask);
        }
    }
#endif

    // 4. Plain AVX2 (32-byte chunks with vector comparison and movemask)
#if defined(__AVX2__) && !(defined(__AVX512VL__) && defined(__AVX512BW__))
    for (; i + 31 < N; i += 32) {
        __m256i l = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(left + i));
        __m256i r = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(right + i));

        __m256i  cmp = _mm256_cmpeq_epi8(l, r);
        uint32_t mask = _mm256_movemask_epi8(cmp);
        if (mask != 0xFFFFFFFF) {
            return i + std::countr_zero(~mask);
        }
    }
#endif

    for (; i + 7 < N; i += 8) {
        uint64_t l, r;
        memcpy(&l, left + i, sizeof(uint64_t));
        memcpy(&r, right + i, sizeof(uint64_t));
        if (l == r) {
            continue;
        }
        return i + std::countr_zero(l ^ r) / 8;
    }

    for (; i < N; i++) {
        if (left[i] != right[i]) {
            return i;
        }
    }
    return N;
}

}  // namespace strmatch
