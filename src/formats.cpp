#include "formats.h"

#include <format>
#include <stdexcept>

namespace formats {
format format::get_main() {
    return format{main::write_format_mark, main::write_block, main::read_block};
}

format format::get_for_mark(uint8_t mark) {
    if (mark == MAIN) {
        return get_main();
    }
    throw std::runtime_error(std::format("Unknown format mark: {}", (int)mark));
}

format format::get_for_option(const std::string& option) {
    if (option == "main") {
        return get_main();
    }
    throw std::runtime_error("Unknown format option");
}
}  // namespace formats
