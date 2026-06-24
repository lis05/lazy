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
#ifdef LZMPODEBUG
        int bits_to_read = static_cast<int>(bits) - bits_left;
        if (bits_to_read > 32) {
            throw std::runtime_error(
                std::format("bits_to_read={} bits={} bits_left={} > 32",
                            bits_to_read, bits, bits_left));
        }
#endif
#if 0
        switch (bits_to_read) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[0])) << bits_left;
            ++in;
            bits_left += 8;
            break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[0])) << bits_left;
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[1]))
                   << (bits_left + 8);
            in += 2;
            bits_left += 16;
            break;
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[0])) << bits_left;
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[1]))
                   << (bits_left + 8);
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[2]))
                   << (bits_left + 16);
            in += 3;
            bits_left += 24;
            break;
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[0])) << bits_left;
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[1]))
                   << (bits_left + 8);
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[2]))
                   << (bits_left + 16);
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(in[3]))
                   << (bits_left + 24);
            in += 4;
            bits_left += 32;
            break;
        }
#endif
#if 0
        while (bits_left < static_cast<int>(bits)) {
            buf |= static_cast<uint64_t>(static_cast<uint8_t>(*in)) << bits_left;
            ++in;
            bits_left += 8;
        }
#endif
#if 1
        // assuming bits <= 32, which is true in our case :3
        if (bits_left < static_cast<int>(bits)) {
            uint32_t new_data;
            std::memcpy(&new_data, in, sizeof(new_data));
            buf |= static_cast<uint64_t>(new_data) << bits_left;
            in += sizeof(new_data);
            bits_left += 32;
        }
#endif

        uint64_t value = buf & ((uint64_t{1} << bits) - 1);

        buf >>= bits;
        bits_left -= static_cast<int>(bits);

        return value;
    }
};
