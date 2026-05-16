#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace strmatch {
static inline size_t match(std::byte *left, std::byte *right, size_t right_limit) {
    assert(left <= right);
    for (size_t i = 0; i < right_limit; i++) {
        if (left[i] != right[i]) {
            return i;
        }
    }
    return right_limit;
}
}  // namespace strmatch
