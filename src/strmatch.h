#pragma once

#include <immintrin.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace strmatch {
template <uint32_t N>
static inline uint32_t match_ctzll(std::byte *left, std::byte *right) {
    uint32_t i = 0;
    for (; i + 7 < N; i += 8) {
        uint64_t l, r;
        memcpy(&l, left + i, sizeof(uint64_t));
        memcpy(&r, right + i, sizeof(uint64_t));
        if (l == r) {
            continue;
        }
        return i + __builtin_ctzll(l ^ r) / 8;
    }
    for (; i < N; i++) {
        if (left[i] != right[i]) {
            return i;
        }
    }
    return N;
}

template <int N>
static inline uint32_t match_simd(const std::byte *left, const std::byte *right) {
    int i = 0;
#if defined(__AVX512F__)
    for (; i + 63 < N; i += 64) {
        __m512i l = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(left + i));
        __m512i r = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(right + i));

        auto mask = _mm512_cmpeq_epi8_mask(l, r);

        if (mask != ~0x0ULL) {
            return i + __builtin_ctzll(~mask);
        }
    }
#endif

#if defined(__AVX2__)
    for (; i + 31 < N; i += 32) {
        __m256i l = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(left + i));
        __m256i r = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(right + i));

        __m256i  cmp = _mm256_cmpeq_epi8(l, r);
        uint32_t mask = _mm256_movemask_epi8(cmp);

        if (mask != ~0x0UL) {
            return i + __builtin_ctz(~mask);
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
        return i + __builtin_ctzll(l ^ r) / 8;
    }
    for (; i < N; i++) {
        if (left[i] != right[i]) {
            return i;
        }
    }
    return N;
}

template <uint32_t N>
static inline uint32_t match(std::byte *left, std::byte *right) {
    for (uint32_t i = 0; i < N; i++) {
        if (left[i] != right[i]) {
            return i;
        }
    }
    return N;
}

template <size_t... N>
static constexpr auto generate_match_table(std::index_sequence<N...>) {
    return std::array{(&match_simd<static_cast<uint32_t>(N)>)...};
}

static constexpr auto match_table =
    generate_match_table(std::make_index_sequence<259>{});

static inline auto get_match(int N) {
    return match_table[N];
}
}  // namespace strmatch
