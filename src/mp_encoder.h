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
#include "hashes.h"
#include "strmatch.h"
#include "table.h"
#include "token.h"

class mp_encoder {
    size_t                 bytes_loaded;
    std::vector<std::byte> data;
    std::vector<token>     tokens;

    table                              head;
    std::vector<std::vector<uint32_t>> prev;

public:
    static constexpr auto sizes = std::to_array<size_t>(
        {6, 10, 14, 18, 22, 26, 32, 36, 40, 44, 48, 52, 56, 58, 62, 66, 70, 74, 78, 82, 86, 90, 94, 98, 102, 106, 110, 114, 118, 122, 126, 130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190, 194, 198, 202, 206, 210, 214, 218, 222, 226, 230, 234, 238, 242, 246, 250, 254});

public:
    mp_encoder();

    std::pair<std::byte *, size_t &> for_loading();

private:
    static constexpr uint32_t literal_cost = 8;

    static inline uint32_t estimate_cost(uint32_t dist, uint32_t len) {
        return literal_cost + 1.5 * (32 - std::countl_zero(dist)) +
               (32 - std::countl_zero(len));
    }

    void process(auto future_limit, const auto NONE, auto i, auto &best_match_len,
                 auto &best_match_pos);

public:
    std::vector<token> encode();
};
