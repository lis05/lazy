#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>

#include "config.h"
#include "token.h"

class decoder {
    std::vector<std::byte> data;

public:
    decoder();

    void reset();
    std::pair<const std::byte *, size_t> get_bytes() const noexcept;
    void                           decode(const std::vector<token> &tokens);
};
