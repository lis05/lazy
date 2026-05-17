#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace strmatch {
template <int N>
static inline uint32_t match(std::byte *left, std::byte *right) {
    for (int i = 0; i < N; i++) {
        if (left[i] != right[i]) {
            return i;
        }
    }
    return N;
}

template <size_t... N>
static constexpr auto generate_match_table(std::index_sequence<N...>) {
    return std::array{(&match<static_cast<int>(N)>)...};
}

static constexpr auto match_table = generate_match_table(std::make_index_sequence<259>{});

static inline auto get_match(int N) {
    return match_table[N];
}
}  // namespace strmatch
