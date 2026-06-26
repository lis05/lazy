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
[[gnu::always_inline]] static inline uint32_t match(uint32_t         N,
                                                    const std::byte *left,
                                                    const std::byte *right) {
    for (uint32_t i = 0; i < N; i += 32) {
        __m256i l = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(left + i));
        __m256i r = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(right + i));

        __mmask32 mask = _mm256_cmpeq_epi8_mask(l, r);
        if (mask != 0xFFFFFFFF) {
            return i + std::countr_zero(~mask);
        }
    }
    return N;
}

}  // namespace strmatch

