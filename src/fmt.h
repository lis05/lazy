#pragma once

#include <fstream>
#include <vector>

#include "token.h"

namespace formats::main {
struct header {
    uint64_t orig_bytes;
    uint32_t n_control;
    uint32_t n_lit;
    uint32_t n_dist;
    uint32_t n_len;
    uint32_t bytes_control;
    uint32_t bytes_lit;
    uint32_t bytes_dist;
    uint32_t bytes_len;
    uint32_t bytes_extra_dist;

    friend std::ostream& operator<<(std::ostream& out, const header& h);
    static const std::byte* read(header &h, const std::byte* ptr);
};

struct streams {
    std::vector<std::byte> controls;
    std::vector<std::byte> literals;
    std::vector<uint32_t>  distances;
    std::vector<uint8_t>   lengths;
};

void verify_config();
void write_format_mark(std::ostream& out);
void write_block(const std::vector<token>& tokens, std::ostream& out);
std::pair<uint64_t, streams> read_block(const std::byte* ptr);
}  // namespace formats::main
