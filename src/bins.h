#pragma once
#include <cstddef>
#include <cstdint>

#include <bit>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>

// clang-format off
//
// First 2^ReservedBits bins match values 1:1. Next Slots bins contain 2 values and
// emit 1 extra bit. Next Slots bins contain 4 values and emit 2 extra bits. And so
// on up until the number of bins reachex MaxBins.
//
// clang-format on
#include <bit>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>

template <uint64_t ReservedBits, uint64_t Slots, uint64_t MaxBins = 256>
struct bins_cfg {
    static constexpr uint64_t MAX_RESERVED = ((uint64_t)1 << ReservedBits) - 1;

    static constexpr uint64_t blog2(uint64_t x) {
        return x == 0 ? 0 : 63 - std::countl_zero(x);
    }

    static constexpr uint64_t MAX_VAL() {
        if (MaxBins <= MAX_RESERVED + 1) {
            return MaxBins - 1;
        }

        uint64_t c = MaxBins - 1;
        uint64_t c_prime = c - MAX_RESERVED - 1;
        uint64_t extra_bits = (c_prime / Slots) + 1;
        uint64_t offset = c_prime % Slots;
        uint64_t Vk = MAX_RESERVED + 1 + Slots * ((1ULL << extra_bits) - 2);
        uint64_t base = Vk + (offset << extra_bits);
        uint64_t max_bin_val = base + (1ULL << extra_bits) - 1;

        uint64_t limit32 = std::numeric_limits<uint32_t>::max();

        if (max_bin_val < limit32) {
            return max_bin_val;
        }

        uint64_t x = limit32 - 1;
        uint64_t Y = x - MAX_RESERVED - 1 + 2 * Slots;
        uint64_t x_extra = blog2(Y / Slots);
        uint64_t x_Vk = MAX_RESERVED + 1 + Slots * ((1ULL << x_extra) - 2);
        uint64_t x_offset = (x - x_Vk) >> x_extra;
        uint64_t x_base = x_Vk + (x_offset << x_extra);
        uint64_t x_max_val = x_base + (1ULL << x_extra) - 1;

        if (x_max_val < limit32) {
            return x_max_val;
        } else {
            uint64_t ctx = MAX_RESERVED + 1 + (x_extra - 1) * Slots + x_offset;
            ctx -= 1;
            uint64_t p_prime = ctx - MAX_RESERVED - 1;
            uint64_t p_extra = (p_prime / Slots) + 1;
            uint64_t p_offset = p_prime % Slots;
            uint64_t p_Vk = MAX_RESERVED + 1 + Slots * ((1ULL << p_extra) - 2);
            uint64_t p_base = p_Vk + (p_offset << p_extra);
            return p_base + (1ULL << p_extra) - 1;
        }
    }

    struct info {
        uint64_t ctx;
        uint64_t extra_bits;
        uint64_t base;
    };

    static inline info get(uint64_t x) {
        if (x <= MAX_RESERVED) {
            return info{x, 0, x};
        }

        if (x > MAX_VAL()) {
            throw std::runtime_error(std::format(
                "Bad number {}. Cannot process: too large ctx/value range. :C", x));
        }

        uint64_t Y = x - MAX_RESERVED - 1 + 2 * Slots;
        uint64_t extra_bits = blog2(Y / Slots);
        uint64_t Vk = MAX_RESERVED + 1 + Slots * ((1ULL << extra_bits) - 2);
        uint64_t offset = (x - Vk) >> extra_bits;
        uint64_t ctx = MAX_RESERVED + 1 + (extra_bits - 1) * Slots + offset;
        uint64_t base = Vk + (offset << extra_bits);

        return info{ctx, extra_bits, base};
    }

    static inline info get_from_ctx(uint64_t c) {
        if (c <= MAX_RESERVED) {
            return info{c, 0, c};
        }

        if (c >= MaxBins) {
            throw std::runtime_error(std::format(
                "Bad context {}. Cannot process: too large ctx/value range. :C", c));
        }

        uint64_t c_prime = c - MAX_RESERVED - 1;
        uint64_t extra_bits = (c_prime / Slots) + 1;
        uint64_t offset = c_prime % Slots;
        uint64_t Vk = MAX_RESERVED + 1 + Slots * ((1ULL << extra_bits) - 2);
        uint64_t base = Vk + (offset << extra_bits);

        if (base > MAX_VAL()) {
            throw std::runtime_error(std::format(
                "Bad context {}. Cannot process: too large ctx/value range. :C", c));
        }

        return info{c, extra_bits, base};
    }
};

#if 0
template <uint64_t ReservedBits, uint64_t Slots, uint64_t MaxBins = 256>
struct bins_cfg {
    static constexpr uint64_t MAX_RESERVED = ((uint64_t)1 << ReservedBits) - 1;
    static constexpr uint64_t MAX_VAL() {
        uint64_t ctx = MAX_RESERVED + 1;
        uint64_t slot = 0;
        uint64_t value = ctx;
        uint64_t value_range = 2;

        uint64_t last_good = 0;

        while (ctx < MaxBins &&
               value + value_range - 1 < std::numeric_limits<uint32_t>::max()) {
            last_good = value + value_range - 1;
            ctx++;
            slot++;
            value += value_range;
            if (slot == Slots) {
                slot = 0;
                value_range <<= 1;
            }
        }

        return last_good;
    }

    struct info {
        uint64_t ctx;
        uint64_t extra_bits;
        uint64_t base;
    };

    static constexpr uint64_t blog2(uint64_t x) {
        return x == 0 ? 0 : 63 - std::countl_zero(x);
    }

    static inline info get(uint64_t x) {
        if (x <= MAX_RESERVED) {
            return info{x, 0, x};
        }

        uint64_t ctx = MAX_RESERVED + 1;
        uint64_t slot = 0;
        uint64_t value = ctx;
        uint64_t value_range = 2;

        while (ctx < MaxBins && value + value_range - 1 < MAX_VAL()) {
            if (value <= x && x < value + value_range) {
                return info{ctx, blog2(value_range), value};
            }
            ctx++;
            slot++;
            value += value_range;
            if (slot == Slots) {
                slot = 0;
                value_range <<= 1;
            }
        }

        throw std::runtime_error(std::format(
            "Bad number {}. Cannot process: too large ctx/value range. :C", x));
        return {};
    }

    static inline info get_from_ctx(uint64_t c) {
        if (c <= MAX_RESERVED) {
            return info{c, 0, c};
        }

        uint64_t ctx = MAX_RESERVED + 1;
        uint64_t slot = 0;
        uint64_t value = ctx;
        uint64_t value_range = 2;

        while (ctx < MaxBins && value + value_range - 1 < MAX_VAL()) {
            if (ctx == c) {
                return info{ctx, blog2(value_range), value};
            }
            ctx++;
            slot++;
            value += value_range;
            if (slot == Slots) {
                slot = 0;
                value_range <<= 1;
            }
        }

        throw std::runtime_error(std::format(
            "Bad context {}. Cannot process: too large ctx/value range. :C", c));
        return {};
    }
};
#endif

using dist_bins = bins_cfg<3, 8>;

