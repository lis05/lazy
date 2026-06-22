#pragma once
#include <cassert>
#include <concepts>
#include <cstring>
#include <iostream>
#include <iterator>

// for little endian only

template <typename It>
    requires std::output_iterator<It, std::byte>
class bit_writer {
    It       out;
    uint64_t buf;
    int      bits_accumulated;

public:
    bit_writer(It out) : out(out), buf(0), bits_accumulated(0) {
    }

    inline void write(uint64_t value, size_t bits) {
        if (bits == 0)
            return;

        // Prevent UB when shifting by 64
        value &= (bits == 64) ? ~uint64_t{0} : (uint64_t{1} << bits) - 1;

        if (bits_accumulated + bits >= 64) {
            int space_left = 64 - bits_accumulated;

            if (space_left < 64) {
                buf |= value << bits_accumulated;
            } else {
                buf = value;
            }

            // Safe for all output iterators.
            // Compilers will optimize to a single movq for contiguous iterators.
            auto* p = reinterpret_cast<const std::byte*>(&buf);
            for (int i = 0; i < 8; ++i) {
                *out = p[i];
                ++out;
            }

            value >>= space_left;
            bits -= space_left;

            buf = 0;
            bits_accumulated = 0;
        }

        if (bits > 0) {
            buf |= value << bits_accumulated;
            bits_accumulated += bits;
        }
    }

    inline void flush() {
        if (bits_accumulated > 0) {
            int   bytes_to_write = (bits_accumulated + 7) / 8;
            auto* p = reinterpret_cast<const std::byte*>(&buf);
            for (int i = 0; i < bytes_to_write; ++i) {
                *out = p[i];
                ++out;
            }
            buf = 0;
            bits_accumulated = 0;
        }
    }
};

template <typename It>
    requires std::input_iterator<It> &&
             std::same_as<std::iter_value_t<It>, std::byte>
class bit_reader {
    It       in;
    uint64_t buf;
    int      bits_left;

public:
    bit_reader(It in) : in(in), buf(0), bits_left(0) {
    }

    inline uint64_t read(size_t bits) {
        if (bits == 0)
            return 0;

        uint64_t value = 0;

        if (bits_left < bits) {
            value = buf;
            size_t needed = bits - bits_left;

            uint64_t next = 0;
            auto*    p = reinterpret_cast<std::byte*>(&next);

            // Safe for all input iterators.
            for (int i = 0; i < 8; ++i) {
                p[i] = *in;
                ++in;
            }

            if (needed == 64) {
                value = next;
                buf = 0;
                bits_left = 0;
            } else {
                value |= (next & ((uint64_t{1} << needed) - 1)) << bits_left;
                buf = next >> needed;
                bits_left = 64 - needed;
            }
        } else {
            value = buf & ((bits == 64) ? ~uint64_t{0} : (uint64_t{1} << bits) - 1);
            buf >>= bits;
            bits_left -= bits;
        }

        return value;
    }
};
