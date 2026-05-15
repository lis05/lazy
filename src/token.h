#pragma once

#include <cstddef>
#include <variant>

struct match {
    size_t distance;
    size_t length;
};

using token = std::variant<match, std::byte>;
