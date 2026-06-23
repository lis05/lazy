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

        buf |= value << bits_accumulated;
        bits_accumulated += bits;

        if (bits_accumulated >= 64) {
            // Safe for all output iterators.
            auto* p = reinterpret_cast<const std::byte*>(&buf);
            for (int i = 0; i < 8; ++i) {
                *out = p[i];
                ++out;
            }

            int excess_bits = bits_accumulated - 64;
            if (excess_bits > 0) {
                buf = value >> (bits - excess_bits);
                bits_accumulated = excess_bits;
            } else {
                buf = 0;
                bits_accumulated = 0;
            }
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

        // Refill byte-by-byte to maintain exact stream position synchronization
        while (bits_left < static_cast<int>(bits)) {
            uint64_t byte_val = static_cast<uint64_t>(static_cast<uint8_t>(*in));
            ++in;
            buf |= byte_val << bits_left;
            bits_left += 8;
        }

        uint64_t value =
            buf & ((bits == 64) ? ~uint64_t{0} : (uint64_t{1} << bits) - 1);

        if (bits == 64) {
            buf = 0;
        } else {
            buf >>= bits;
        }
        bits_left -= bits;

        return value;
    }
};

