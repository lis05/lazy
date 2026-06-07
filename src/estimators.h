#pragma once
#include <bit>
#include <concepts>

namespace estimators {
template <std::integral T>
static constexpr T literal_cost = 8*8;

template <std::integral T>
static inline T cost(T dist, T len) {
    constexpr T full_bits = 8 * sizeof(T);
    return literal_cost<T> + 10 * (full_bits - std::countl_zero(dist)) +
           8 * (full_bits - std::countl_zero(len));
}
}  // namespace estimators
