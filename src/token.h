#pragma once

#include <cstddef>
#include <variant>

struct match {
    uint32_t distance;
    uint32_t length;
    auto     operator<=>(const match&) const = default;
};

using token = std::variant<match, std::byte>;
