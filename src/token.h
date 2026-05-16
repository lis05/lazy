#pragma once

#include <cstddef>
#include <variant>

struct match {
    uint32_t distance;
    uint32_t length;
};

using token = std::variant<match, std::byte>;
