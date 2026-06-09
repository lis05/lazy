#pragma once
#include <bit>
#include <concepts>

#include "bins.h"

namespace estimators {
namespace simple {
template <std::integral T>
static constexpr T literal_cost = 8 * 8;

template <std::integral T>
static inline T cost(T dist, T len) {
    constexpr T full_bits = 8 * sizeof(T);
    return literal_cost<T> + 10 * (full_bits - std::countl_zero(dist)) +
           8 * (full_bits - std::countl_zero(len));
}
}  // namespace simple

namespace better {
struct state {
    uint32_t dist_cache[3];
    state(uint32_t dist0, uint32_t dist1, uint32_t dist2) {
        dist_cache[0] = dist0;
        dist_cache[1] = dist1;
        dist_cache[2] = dist2;
    }
};

template <std::integral T>
static constexpr T control_cost = 1;

template <std::integral T>
static constexpr T literal_cost = 64;

template <std::integral T>
static inline T cost(T dist, T len, const state &s) {
    constexpr T full_bits = 8 * sizeof(T);

    T len_cost = 8 * (full_bits - std::countl_zero(len));

    if (s.dist_cache[0] == dist || s.dist_cache[1] == dist ||
        s.dist_cache[2] == dist) {
        return 8 * control_cost<T> + len_cost;
    } else {
        return literal_cost<T> + 10 * (full_bits - std::countl_zero(dist)) +
               8 * (full_bits - std::countl_zero(len));
    }
}
}  // namespace better
}  // namespace estimators
