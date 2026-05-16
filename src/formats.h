#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <variant>
#include <vector>

#include "bitstream.h"
#include "token.h"

namespace formats {

enum marks : uint8_t {
    READABLE,
    BINARY
};

using write_format_mark_fn = void (*)(std::ofstream&);
using write_block_fn = void (*)(const std::vector<token>& tokens, std::ofstream&);
using read_block_fn = std::vector<token> (*)(std::ifstream&);

namespace readable {
struct header {
    uint32_t     n_items;
    friend auto& operator<<(auto& out, const header& h) {
        return out << h.n_items << " ";
    }
    friend auto& operator>>(auto& in, header& h) {
        return in >> h.n_items;
    }
};

static void write_format_mark(std::ofstream& out) {
    out << READABLE;
}

static void write_block(const std::vector<token>& tokens, std::ofstream& out) {
    out << header{tokens.size()};

    for (auto& t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            out << "B ";
            out << static_cast<int>(std::get<std::byte>(t)) << "\n";
        } else {
            auto m = std::get<match>(t);
            out << "M " << m.distance << " " << m.length << "\n";
        }
    }
}

static std::vector<token> read_block(std::ifstream& in) {
    std::vector<token> res;
    header             h;
    if (!(in >> h)) {
        return {};
    }

    for (uint32_t i = 0; i < h.n_items; i++) {
        char c;
        in >> c;

        if (c == 'B') {
            int b;
            in >> b;
            res.push_back(std::byte{b});
        } else if (c == 'M') {
            match m;
            in >> m.distance >> m.length;
            res.push_back(m);
        }
    }

    return res;
}
};  // namespace readable

namespace binary {
struct header {
    uint32_t     n_items;
    friend auto& operator<<(auto& out, const header& h) {
        return out.write(reinterpret_cast<const char*>(&h.n_items),
                         sizeof(h.n_items));
    }
    friend auto& operator>>(auto& in, header& h) {
        return in.read(reinterpret_cast<char*>(&h.n_items), sizeof(h.n_items));
    }
};

static void write_format_mark(std::ofstream& out) {
    out << BINARY;
}

static inline unsigned char get_byte(uint32_t num, int pos) {
    return (num >> (8 * pos)) & 0xFFu;
}

static inline void set_byte(uint32_t& num, int pos, unsigned char b) {
    num |= (static_cast<uint32_t>(b) << (8 * pos));
}

static void write_block(const std::vector<token>& tokens, std::ofstream& out) {
    out << header{tokens.size()};

    bit_writer writer(out);

    for (auto& t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            writer.write_bit(0);
            writer.write(static_cast<unsigned char>(std::get<std::byte>(t)), 8);
        } else {
            auto m = std::get<match>(t);
            if (m.length == 3 && m.distance < 256) {
                writer.write_bit(1);
                writer.write_bit(0);
                writer.write_bit(0);
                writer.write(m.distance, 8);
            } else if (m.length == 4 && m.distance < 256) {
                writer.write_bit(1);
                writer.write_bit(0);
                writer.write_bit(1);
                writer.write(m.distance, 8);
            } else if (m.length == 5 && m.distance < 1024) {
                writer.write_bit(1);
                writer.write_bit(1);
                writer.write_bit(0);
                writer.write_bit(0);
                writer.write(m.distance, 10);
            } else if (m.length == 6 && m.distance < 1024) {
                writer.write_bit(1);
                writer.write_bit(1);
                writer.write_bit(0);
                writer.write_bit(1);
                writer.write(m.distance, 10);
            } else if (m.length == 7 && m.distance < 1024) {
                writer.write_bit(1);
                writer.write_bit(1);
                writer.write_bit(1);
                writer.write_bit(0);
                writer.write(m.distance, 10);
            } else {
                assert(m.length - 1 < 16);
                assert(m.distance - 1 < (1 << 15));
                writer.write_bit(1);
                writer.write_bit(1);
                writer.write_bit(1);
                writer.write_bit(1);
                writer.write(m.length - 1, 4);
                writer.write(m.distance - 1, 15);
            }
        }
    }

    writer.flush();
}

static std::vector<token> read_block(std::ifstream& in) {
    header h;
    in >> h;

    std::vector<token> tokens;
    tokens.reserve(h.n_items);

    bit_reader reader(in);

    for (uint32_t i = 0; i < h.n_items; ++i) {
        int b1 = reader.read_bit();

        if (b1 == 0) {
            tokens.push_back(static_cast<std::byte>(reader.read<unsigned char>(8)));
        } else {
            uint32_t length = 0;
            uint32_t distance = 0;

            int b2 = reader.read_bit();
            if (b2 == 0) {
                int b3 = reader.read_bit();
                length = (b3 == 0) ? 3 : 4;
                distance = reader.read<uint32_t>(8);
            } else {
                int b3 = reader.read_bit();
                if (b3 == 0) {
                    int b4 = reader.read_bit();
                    length = (b4 == 0) ? 5 : 6;
                    distance = reader.read<uint32_t>(10);
                } else {
                    int b4 = reader.read_bit();
                    if (b4 == 0) {
                        length = 7;
                        distance = reader.read<uint32_t>(10);
                    } else {
                        length = reader.read<uint32_t>(4) + 1;
                        distance = reader.read<uint32_t>(15) + 1;
                    }
                }
            }
            assert(length > 0);
            assert(distance > 0);
            tokens.push_back(match{distance, length});
        }
    }

    return tokens;
}
}  // namespace binary
struct format {
    write_format_mark_fn write_format_mark;
    write_block_fn       write_block;
    read_block_fn        read_block;

    static format get_readable() {
        return format{readable::write_format_mark, readable::write_block,
                      readable::read_block};
    }

    static format get_binary() {
        return format{binary::write_format_mark, binary::write_block,
                      binary::read_block};
    }

    static format get_for_mark(uint8_t mark) {
        if (mark == READABLE) {
            return get_readable();
        } else if (mark == BINARY) {
            return get_binary();
        }
        throw std::runtime_error("Unknown format mark");
    }

    static format get_for_option(const std::string& option) {
        if (option == "readable") {
            return get_readable();
        } else if (option == "binary") {
            return get_binary();
        }
        throw std::runtime_error("Unknown format option");
    }
};

}  // namespace formats
