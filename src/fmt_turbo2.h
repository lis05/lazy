#pragma once

#include <fstream>
#include <vector>

#include "token.h"

namespace formats::turbo2 {
struct header {
    uint32_t n_control;
    uint32_t n_lit;
    uint32_t n_dist;
    uint32_t n_len;
    uint32_t bytes_control;
    uint32_t bytes_lit;
    uint32_t bytes_dist;
    uint32_t bytes_len;

    friend std::istream& operator>>(std::istream& in, header& h);
    friend std::ostream& operator<<(std::ostream& out, const header& h);
};

void               verify_config();
void               write_format_mark(std::ostream& out);
void               write_block(const std::vector<token>& tokens, std::ostream& out);
std::vector<token> read_block(std::istream& in);
}  // namespace formats::turbo2
