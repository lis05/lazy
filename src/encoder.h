#pragma once
#include <algorithm>
#include <array>
#include <ext/pb_ds/assoc_container.hpp>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "config.h"
#include "estimators.h"
#include "token.h"

struct encoder {
    size_t                             bytes_loaded;
    std::vector<std::byte>             data;
    std::vector<std::vector<uint32_t>> chains;
    std::vector<std::vector<uint32_t>> tables;
    std::vector<token>                 tokens;

    bool are_tokens_available;

    void               load(const std::byte *from, uint32_t count);
    void               reset_for_next_pass(uint32_t pass);
    std::vector<token> encode(uint32_t pass, estimators::smart &est);
};
