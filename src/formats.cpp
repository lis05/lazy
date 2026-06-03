#include "formats.h"

#include <format>
#include <stdexcept>

namespace formats {
format format::get_readable() {
    return format{readable::write_format_mark, readable::write_block,
                  readable::read_block, readable::verify_config};
}

format format::get_ctx() {
    return format{ctx::write_format_mark, ctx::write_block, ctx::read_block,
                  ctx::verify_config};
}

format format::get_turbo() {
    return format{turbo::write_format_mark, turbo::write_block, turbo::read_block,
                  turbo::verify_config};
}

format format::get_turbo2() {
    return format{turbo2::write_format_mark, turbo2::write_block, turbo2::read_block,
                  turbo2::verify_config};
}

format format::get_for_mark(uint8_t mark) {
    if (mark == READABLE) {
        return get_readable();
    } else if (mark == CTX) {
        return get_ctx();
    } else if (mark == TURBO) {
        return get_turbo();
    } else if (mark == TURBO2) {
        return get_turbo2();
    }
    throw std::runtime_error(std::format("Unknown format mark: {}", (int)mark));
}

format format::get_for_option(const std::string& option) {
    if (option == "readable") {
        return get_readable();
    } else if (option == "ctx") {
        return get_ctx();
    } else if (option == "turbo") {
        return get_turbo();
    } else if (option == "turbo2") {
        return get_turbo2();
    }
    throw std::runtime_error("Unknown format option");
}
}  // namespace formats
