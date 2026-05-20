#pragma once

#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "config.h"
#include "token.h"

class encoder {
    size_t                 bytes_loaded;
    std::vector<std::byte> data;
    std::vector<token>     tokens;
    std::vector<uint32_t>  head;
    std::vector<uint32_t>  prev;

public:
    encoder();
    std::pair<std::byte *, size_t &> for_loading();
    void                             reset();
    const std::vector<token>         encode();
};
