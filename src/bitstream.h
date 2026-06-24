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
        for (int i = 0; i < 16; ++i) {
            *out = std::byte{0};
            ++out;
        }
    }
};

class bit_reader {
    const std::byte* in;
    uint64_t         buf;
    int              bits_left;

public:
    explicit bit_reader(const std::byte* in) : in(in), buf(0), bits_left(0) {
    }

    inline uint64_t read(size_t bits) {
        // assuming bits <= 32, which is true in our case :3
        if (bits_left < static_cast<int>(bits)) {
            uint32_t new_data;
            std::memcpy(&new_data, in, sizeof(new_data));
            buf |= static_cast<uint64_t>(new_data) << bits_left;
            in += sizeof(new_data);
            bits_left += 32;
        }

        uint64_t value = buf & ((uint64_t{1} << bits) - 1);

        buf >>= bits;
        bits_left -= static_cast<int>(bits);

        return value;
    }
};
