#pragma once
#include <fstream>
#include <vector>

#include "formats.h"
#include "token.h"

namespace formats::readable {
struct header {
    uint32_t             n_items;
    friend std::ostream& operator<<(std::ostream& out, const header& h);
    friend std::istream& operator>>(std::istream& in, header& h);
};

void               verify_config();
void               write_format_mark(std::ostream& out);
void               write_block(const std::vector<token>& tokens, std::ostream& out);
std::vector<token> read_block(std::istream& in);
};  // namespace formats::readable
