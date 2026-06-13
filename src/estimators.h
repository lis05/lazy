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

namespace _stupid_but_works {
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
}  // namespace _stupid_but_works
namespace _smart_but_freaking_sucks {
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
static constexpr T literal_cost = 7;

template <std::integral T>
static inline T cost(T dist, T len, const state &s) {
    constexpr T full_bits = 8 * sizeof(T);

    T len_cost = full_bits - std::countl_zero(len);

    if (s.dist_cache[0] == dist || s.dist_cache[1] == dist ||
        s.dist_cache[2] == dist) {
        return control_cost<T> + len_cost;
    } else {
        auto info = dist_bins::get(dist);
        constexpr auto ctx_cost =
            full_bits - std::countl_zero(static_cast<T>(dist_bins::get(1e9).ctx));
        return control_cost<T> + ctx_cost + info.extra_bits + len_cost;
    }
}
}  // namespace _smart_but_freaking_sucks

namespace better = _smart_but_freaking_sucks;
}  // namespace estimators
