#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>
#include <fstream>
#include <variant>

#include "token.h"

namespace formats {

enum marks : uint8_t {
    READABLE,
};

using write_format_mark_fn = void(*)(std::ofstream&);
using write_block_fn = void(*)(const std::vector<token>& tokens, std::ofstream&);
using read_block_fn = std::vector<token>(*)(std::ifstream&);

namespace readable {
struct header {
    size_t       n_items;
    friend auto& operator<<(auto& out, const header& h) {
        return out << h.n_items;
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
            out << (int)std::get<std::byte>(t) << "\n";
        } else {
            auto m = std::get<match>(t);
            out << "M " << m.distance << " " << m.length << "\n";
        }
    }
}

static std::vector<token> read_block(std::ifstream& in) {
    std::vector<token> res;
    header             h;
    in >> h;

    for (size_t i = 0; i < h.n_items; i++) {
        char c;
        in >> c;

        if (c == 'B') {
            uint8_t b;
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

struct format {
    write_format_mark_fn write_format_mark;
    write_block_fn       write_block;
    read_block_fn        read_block;

    static format get_readable() {
        return format{readable::write_format_mark, readable::write_block,
                      readable::read_block};
    }
};

}  // namespace formats
