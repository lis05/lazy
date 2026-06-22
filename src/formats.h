#pragma once
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <utility>

#include "fmt.h"
#include "token.h"

namespace formats {

enum marks : unsigned char {
    MAIN,
};

using write_format_mark_fn = void (*)(std::ostream&);
using write_block_fn = void (*)(const std::vector<token>& tokens, std::ostream&);
using read_block_fn = std::pair<uint64_t, std::vector<token>> (*)(std::istream&);
using verify_config_fn = void (*)();

struct format {
    write_format_mark_fn write_format_mark;
    write_block_fn       write_block;
    read_block_fn        read_block;
    verify_config_fn     verify_config;

    static format get_main();

    static format get_for_mark(uint8_t mark);
    static format get_for_option(const std::string& option);
};
}  // namespace formats
