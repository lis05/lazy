#pragma once
#include <concepts>
#include <iostream>

class bit_writer {
    std::ofstream &out;
    int            byte;
    int            bits_accumulated;

public:
    bit_writer(std::ofstream &out) : out(out), byte(0), bits_accumulated(0) {
    }

    inline void write_bit(int value) {
        byte ^= (value & 1) << bits_accumulated++;
        if (bits_accumulated == 8) {
            out << static_cast<unsigned char>(byte);
            bits_accumulated = 0;
            byte = 0;
        }
    }

    inline void write(std::unsigned_integral auto value, size_t bits) {
        for (size_t i = 0; i < bits; i++) {
            write_bit(value & 1);
            value >>= 1;
        }
    }

    inline void flush() {
        if (bits_accumulated != 0) {
            out << static_cast<unsigned char>(byte);
        }
    }
};

class bit_reader {
    std::ifstream &in;
    int            byte;
    int            bits_left;

public:
    bit_reader(std::ifstream &in) : in(in), byte(0), bits_left(0) {}

    inline int read_bit() {
        if (bits_left == 0) {
            byte = in.get();
            assert(byte != EOF);
            bits_left = 8;
        }

        int bit = (byte >> (8 - bits_left)) & 1;
        bits_left--;
        return bit;
    }

    template<std::unsigned_integral T>
    inline T read(size_t bits) {
        T value = 0;
        for (size_t i = 0; i < bits; i++) {
            int bit = read_bit();
            if (bit == EOF) {
                break;
            }
            value |= (static_cast<T>(bit) << i);
        }
        return value;
    }
};
