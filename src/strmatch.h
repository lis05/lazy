#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace strmatch {
static inline uint32_t match(std::byte *left, std::byte *right, uint32_t right_limit) {
    assert(left <= right);
    for (uint32_t i = 0; i < right_limit; i++) {
        if (left[i] != right[i]) {
            return i;
        }
    }
    return right_limit;
}
}  // namespace strmatch
