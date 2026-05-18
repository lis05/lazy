#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <variant>
#include <vector>

extern "C" {
#include <fse.h>
}

#include "token.h"

namespace formats {

enum marks : unsigned char {
    READABLE,
    FSE
};

using write_format_mark_fn = void (*)(std::ofstream&);
using write_block_fn = void (*)(const std::vector<token>& tokens, std::ofstream&);
using read_block_fn = std::vector<token> (*)(std::ifstream&);
using verify_config_fn = void (*)();

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

static void verify_config() {
    if (config::block_size == 0) {
        throw std::runtime_error("Invalid block size: 0");
    }

    if (config::window_size == 0) {
        throw std::runtime_error("Invalid window size: 0");
    }

    if (config::future_limit == 0) {
        throw std::runtime_error("Invalid future limit: 0");
    }
}
};  // namespace readable

namespace fse {
struct header {
    static constexpr int FSE_NORMAL = 0;
    static constexpr int FSE_RAW = 1;
    static constexpr int FSE_RLE = 2;

    uint32_t n_control;
    uint32_t n_literals;
    uint32_t n_matches;

    int control_fse_flag;
    int literals_fse_flag;
    int distances1_fse_flag;
    int distances2_fse_flag;
    int distances3_fse_flag;
    int distances4_fse_flag;
    int lengths_fse_flag;

    uint32_t controls_bytes;
    uint32_t literals_bytes;
    uint32_t distances1_bytes;
    uint32_t distances2_bytes;
    uint32_t distances3_bytes;
    uint32_t distances4_bytes;
    uint32_t lengths_bytes;

    friend auto& operator<<(auto& out, const header& h) {
        out.write(reinterpret_cast<const char*>(&h.n_control), sizeof(h.n_control))
            .write(reinterpret_cast<const char*>(&h.n_literals),
                   sizeof(h.n_literals))
            .write(reinterpret_cast<const char*>(&h.n_matches), sizeof(h.n_matches))
            .write(reinterpret_cast<const char*>(&h.controls_bytes),
                   sizeof(h.controls_bytes))
            .write(reinterpret_cast<const char*>(&h.literals_bytes),
                   sizeof(h.literals_bytes))
            .write(reinterpret_cast<const char*>(&h.distances1_bytes),
                   sizeof(h.distances1_bytes))
            .write(reinterpret_cast<const char*>(&h.distances2_bytes),
                   sizeof(h.distances2_bytes))
            .write(reinterpret_cast<const char*>(&h.distances3_bytes),
                   sizeof(h.distances3_bytes))
            .write(reinterpret_cast<const char*>(&h.distances4_bytes),
                   sizeof(h.distances4_bytes))
            .write(reinterpret_cast<const char*>(&h.lengths_bytes),
                   sizeof(h.lengths_bytes));

        uint16_t flags =
            (h.control_fse_flag & 3) | ((h.literals_fse_flag & 3) << 2) |
            ((h.distances1_fse_flag & 3) << 4) | ((h.distances2_fse_flag & 3) << 6) |
            ((h.distances3_fse_flag & 3) << 8) |
            ((h.distances4_fse_flag & 3) << 10) | ((h.lengths_fse_flag & 3) << 12);

        out.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
        return out;
    }

    friend auto& operator>>(auto& in, header& h) {
        in.read(reinterpret_cast<char*>(&h.n_control), sizeof(h.n_control))
            .read(reinterpret_cast<char*>(&h.n_literals), sizeof(h.n_literals))
            .read(reinterpret_cast<char*>(&h.n_matches), sizeof(h.n_matches))
            .read(reinterpret_cast<char*>(&h.controls_bytes),
                  sizeof(h.controls_bytes))
            .read(reinterpret_cast<char*>(&h.literals_bytes),
                  sizeof(h.literals_bytes))
            .read(reinterpret_cast<char*>(&h.distances1_bytes),
                  sizeof(h.distances1_bytes))
            .read(reinterpret_cast<char*>(&h.distances2_bytes),
                  sizeof(h.distances2_bytes))
            .read(reinterpret_cast<char*>(&h.distances3_bytes),
                  sizeof(h.distances3_bytes))
            .read(reinterpret_cast<char*>(&h.distances4_bytes),
                  sizeof(h.distances4_bytes))
            .read(reinterpret_cast<char*>(&h.lengths_bytes),
                  sizeof(h.lengths_bytes));

        uint16_t flags = 0;
        in.read(reinterpret_cast<char*>(&flags), sizeof(flags));

        h.control_fse_flag = flags & 3;
        flags >>= 2;
        h.literals_fse_flag = flags & 3;
        flags >>= 2;
        h.distances1_fse_flag = flags & 3;
        flags >>= 2;
        h.distances2_fse_flag = flags & 3;
        flags >>= 2;
        h.distances3_fse_flag = flags & 3;
        flags >>= 2;
        h.distances4_fse_flag = flags & 3;
        flags >>= 2;
        h.lengths_fse_flag = flags & 3;
        return in;
    }
};

static void write_format_mark(std::ofstream& out) {
    out << FSE;
}

static void write_block(const std::vector<token>& tokens, std::ofstream& out) {
    header header{.n_control = static_cast<uint32_t>(tokens.size())};
    for (auto t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            header.n_literals++;
        } else {
            header.n_matches++;
        }
    }

    std::vector<uint8_t> controls;
    std::vector<uint8_t> literals;
    std::vector<uint8_t> distances1;
    std::vector<uint8_t> distances2;
    std::vector<uint8_t> distances3;
    std::vector<uint8_t> distances4;
    std::vector<uint8_t> lengths;

    for (auto t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            controls.push_back(0);
            literals.push_back(static_cast<uint8_t>(std::get<std::byte>(t)));
        } else {
            uint32_t dist = static_cast<uint32_t>(std::get<match>(t).distance);
            lengths.push_back(static_cast<uint8_t>(std::get<match>(t).length - 3));

            if (dist <= 0xFF) {
                controls.push_back(1);
                distances1.push_back(static_cast<uint8_t>(dist));
            } else if (dist <= 0xFFFF) {
                controls.push_back(2);
                distances2.push_back(static_cast<uint8_t>(dist & 0xFF));
                distances2.push_back(static_cast<uint8_t>((dist >> 8) & 0xFF));
            } else if (dist <= 0xFFFFFF) {
                controls.push_back(3);
                distances3.push_back(static_cast<uint8_t>(dist & 0xFF));
                distances3.push_back(static_cast<uint8_t>((dist >> 8) & 0xFF));
                distances3.push_back(static_cast<uint8_t>((dist >> 16) & 0xFF));
            } else {
                controls.push_back(4);
                distances4.push_back(static_cast<uint8_t>(dist & 0xFF));
                distances4.push_back(static_cast<uint8_t>((dist >> 8) & 0xFF));
                distances4.push_back(static_cast<uint8_t>((dist >> 16) & 0xFF));
                distances4.push_back(static_cast<uint8_t>((dist >> 24) & 0xFF));
            }
        }
    }

    std::vector<std::byte> out_controls, out_literals, out_lengths;
    std::vector<std::byte> out_distances1, out_distances2, out_distances3,
        out_distances4;

    auto compress_payload = [](const std::vector<uint8_t>& src,
                               std::vector<std::byte>& out_buf, int& out_flag,
                               uint32_t& out_bytes, const char* name) {
        if (src.empty()) {
            out_flag = header::FSE_RAW;
            out_bytes = 0;
            return;
        }
        out_buf.resize(FSE_compressBound(src.size()));
        size_t res =
            FSE_compress(out_buf.data(), out_buf.size(), src.data(), src.size());
        if (FSE_isError(res)) {
            throw std::runtime_error(
                std::string("FSE ") + name +
                " compression failed: " + FSE_getErrorName(res));
        }
        if (res == 0) {
            out_flag = header::FSE_RAW;
            out_bytes = src.size();
        } else if (res == 1) {
            out_flag = header::FSE_RLE;
            out_bytes = 1;
        } else {
            out_flag = header::FSE_NORMAL;
            out_buf.resize(res);
            out_bytes = res;
        }
    };

    compress_payload(controls, out_controls, header.control_fse_flag,
                     header.controls_bytes, "control");
    compress_payload(literals, out_literals, header.literals_fse_flag,
                     header.literals_bytes, "literal");
    compress_payload(distances1, out_distances1, header.distances1_fse_flag,
                     header.distances1_bytes, "distance1");
    compress_payload(distances2, out_distances2, header.distances2_fse_flag,
                     header.distances2_bytes, "distance2");
    compress_payload(distances3, out_distances3, header.distances3_fse_flag,
                     header.distances3_bytes, "distance3");
    compress_payload(distances4, out_distances4, header.distances4_fse_flag,
                     header.distances4_bytes, "distance4");
    compress_payload(lengths, out_lengths, header.lengths_fse_flag,
                     header.lengths_bytes, "length");

    out << header;

    auto write_payload = [&](int flag, uint32_t sz, const void* raw_src,
                             const std::byte* fse_src) {
        if (sz == 0)
            return;
        if (flag == header::FSE_RAW || flag == header::FSE_RLE) {
            out.write(reinterpret_cast<const char*>(raw_src), sz);
        } else {
            out.write(reinterpret_cast<const char*>(fse_src), sz);
        }
    };

    write_payload(header.control_fse_flag, header.controls_bytes, controls.data(),
                  out_controls.data());
    write_payload(header.literals_fse_flag, header.literals_bytes, literals.data(),
                  out_literals.data());
    write_payload(header.distances1_fse_flag, header.distances1_bytes,
                  distances1.data(), out_distances1.data());
    write_payload(header.distances2_fse_flag, header.distances2_bytes,
                  distances2.data(), out_distances2.data());
    write_payload(header.distances3_fse_flag, header.distances3_bytes,
                  distances3.data(), out_distances3.data());
    write_payload(header.distances4_fse_flag, header.distances4_bytes,
                  distances4.data(), out_distances4.data());
    write_payload(header.lengths_fse_flag, header.lengths_bytes, lengths.data(),
                  out_lengths.data());
}

static std::vector<token> read_block(std::ifstream& in) {
    header header;
    if (!(in >> header))
        return {};

    auto read_compressed = [&](uint32_t comp_sz) {
        std::vector<char> buf(comp_sz);
        if (comp_sz > 0)
            in.read(buf.data(), comp_sz);
        return buf;
    };

    auto comp_controls = read_compressed(header.controls_bytes);
    auto comp_literals = read_compressed(header.literals_bytes);
    auto comp_distances1 = read_compressed(header.distances1_bytes);
    auto comp_distances2 = read_compressed(header.distances2_bytes);
    auto comp_distances3 = read_compressed(header.distances3_bytes);
    auto comp_distances4 = read_compressed(header.distances4_bytes);
    auto comp_lengths = read_compressed(header.lengths_bytes);

    std::vector<uint8_t> controls(header.n_control);
    auto decompress_payload = [](int flag, const std::vector<char>& src,
                                 std::vector<uint8_t>& dest) {
        if (src.empty() || dest.empty())
            return;
        if (flag == header::FSE_RAW) {
            std::memcpy(dest.data(), src.data(), dest.size());
        } else if (flag == header::FSE_RLE) {
            std::fill(dest.begin(), dest.end(), static_cast<uint8_t>(src[0]));
        } else {
            size_t res =
                FSE_decompress(dest.data(), dest.size(), src.data(), src.size());
            if (FSE_isError(res)) {
                throw std::runtime_error("FSE decompression failed: " +
                                         std::string(FSE_getErrorName(res)));
            }
        }
    };

    decompress_payload(header.control_fse_flag, comp_controls, controls);

    size_t n_lit = 0, n_dist1 = 0, n_dist2 = 0, n_dist3 = 0, n_dist4 = 0, n_len = 0;
    for (uint8_t c : controls) {
        if (c == 0) {
            n_lit++;
        } else {
            n_len++;
            if (c == 1)
                n_dist1++;
            else if (c == 2)
                n_dist2++;
            else if (c == 3)
                n_dist3++;
            else if (c == 4)
                n_dist4++;
        }
    }

    std::vector<uint8_t> literals(n_lit);
    std::vector<uint8_t> distances1(n_dist1 * 1);
    std::vector<uint8_t> distances2(n_dist2 * 2);
    std::vector<uint8_t> distances3(n_dist3 * 3);
    std::vector<uint8_t> distances4(n_dist4 * 4);
    std::vector<uint8_t> lengths(n_len);

    decompress_payload(header.literals_fse_flag, comp_literals, literals);
    decompress_payload(header.distances1_fse_flag, comp_distances1, distances1);
    decompress_payload(header.distances2_fse_flag, comp_distances2, distances2);
    decompress_payload(header.distances3_fse_flag, comp_distances3, distances3);
    decompress_payload(header.distances4_fse_flag, comp_distances4, distances4);
    decompress_payload(header.lengths_fse_flag, comp_lengths, lengths);

    std::vector<token> tokens;
    tokens.reserve(header.n_control);

    size_t lit_idx = 0, len_idx = 0;
    size_t d1_idx = 0, d2_idx = 0, d3_idx = 0, d4_idx = 0;

    for (uint8_t c : controls) {
        if (c == 0) {
            tokens.push_back(static_cast<std::byte>(literals[lit_idx++]));
        } else {
            uint32_t dist = 0;
            if (c == 1) {
                dist = distances1[d1_idx++];
            } else if (c == 2) {
                dist = distances2[d2_idx] | (distances2[d2_idx + 1] << 8);
                d2_idx += 2;
            } else if (c == 3) {
                dist = distances3[d3_idx] | (distances3[d3_idx + 1] << 8) |
                       (distances3[d3_idx + 2] << 16);
                d3_idx += 3;
            } else if (c == 4) {
                dist = distances4[d4_idx] | (distances4[d4_idx + 1] << 8) |
                       (distances4[d4_idx + 2] << 16) |
                       (distances4[d4_idx + 3] << 24);
                d4_idx += 4;
            }

            tokens.push_back(
                match{.distance = dist, .length = lengths[len_idx++] + 3});
        }
    }

    return tokens;
}
static void verify_config() {
    if (config::block_size == 0) {
        throw std::runtime_error("Invalid block size: 0");
    }

    if (config::window_size == 0) {
        throw std::runtime_error("Invalid window size: 0");
    }

    if (config::future_limit == 0) {
        throw std::runtime_error("Invalid future limit: 0");
    }
    if (config::future_limit > 258) {
        throw std::runtime_error(
            std::format("Invalid future limit: {}. Cannot be greater than 258",
                        config::future_limit));
    }
}
}  // namespace fse
struct format {
    write_format_mark_fn write_format_mark;
    write_block_fn       write_block;
    read_block_fn        read_block;
    verify_config_fn     verify_config;

    static format get_readable() {
        return format{readable::write_format_mark, readable::write_block,
                      readable::read_block, readable::verify_config};
    }

    static format get_fse() {
        return format{fse::write_format_mark, fse::write_block, fse::read_block,
                      fse::verify_config};
    }

    static format get_for_mark(uint8_t mark) {
        if (mark == READABLE) {
            return get_readable();
        } else if (mark == FSE) {
            return get_fse();
        }
        throw std::runtime_error("Unknown format mark");
    }

    static format get_for_option(const std::string& option) {
        if (option == "readable") {
            return get_readable();
        } else if (option == "fse") {
            return get_fse();
        }
        throw std::runtime_error("Unknown format option");
    }
};
}  // namespace formats
