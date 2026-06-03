#pragma once

#include <fstream>
#include <vector>

#include "token.h"

namespace formats::turbo {
struct header {
    uint32_t n_dist_ctx;
    uint32_t n_len_ctx;
    uint32_t n_lit_ctx;
    uint32_t bytes_dist_ctx;
    uint32_t bytes_len_ctx;
    uint32_t bytes_lit_ctx;
    uint32_t bytes_extra;

    friend std::istream& operator>>(std::istream& in, header& h);
    friend std::ostream& operator<<(std::ostream& out, const header& h);
};

void               verify_config();
void               write_format_mark(std::ostream& out);
void               write_block(const std::vector<token>& tokens, std::ostream& out);
std::vector<token> read_block(std::istream& in);

}  // namespace formats::turbo
