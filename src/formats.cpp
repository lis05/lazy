#include "formats.h"

#include <format>
#include <stdexcept>

namespace formats {
format format::get_turbo2() {
    return format{turbo2::write_format_mark, turbo2::write_block, turbo2::read_block,
                  turbo2::verify_config};
}

format format::get_for_mark(uint8_t mark) {
    if (mark == TURBO2) {
        return get_turbo2();
    }
    throw std::runtime_error(std::format("Unknown format mark: {}", (int)mark));
}

format format::get_for_option(const std::string& option) {
    if (option == "turbo2") {
        return get_turbo2();
    }
    throw std::runtime_error("Unknown format option");
}
}  // namespace formats
