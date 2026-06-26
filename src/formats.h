#pragma once
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "fmt.h"
#include "token.h"

namespace formats {

enum marks : unsigned char {
    MAIN,
};

using write_format_mark_fn = std::byte* (*)(std::byte * ptr);
using write_block_fn = std::byte* (*)(const std::vector<token>& tokens,
                                      std::byte*                ptr);
using read_block_fn = std::pair<uint64_t, main::streams> (*)(const std::byte* ptr);

struct format {
    write_format_mark_fn write_format_mark;
    write_block_fn       write_block;
    read_block_fn        read_block;

    static format get_main();

    static format get_for_mark(uint8_t mark);
    static format get_for_option(const std::string& option);
};
}  // namespace formats
