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

    static std::byte* write(const header& h, std::byte* ptr);
    static const std::byte* read(header& h, const std::byte* ptr);
};

struct streams {
    uint32_t   n_controls;
    std::byte* controls;
    std::byte* literals;
    uint32_t*  distances;
    uint8_t*   lengths;
};

std::byte* write_format_mark(std::byte* ptr);
std::byte* write_block(const std::vector<token>& tokens, std::byte* ptr);
std::pair<uint64_t, streams> read_block(const std::byte* ptr);
}  // namespace formats::main
