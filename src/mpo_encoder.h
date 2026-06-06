#pragma once
#include <algorithm>
#include <array>
#include <compact_vector/include/compact_vector.hpp>
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
#include "hashes.h"
#include "strmatch.h"
#include "table.h"
#include "token.h"

class mpo_encoder {
    size_t                 bytes_loaded;
    std::vector<std::byte> data;
    std::vector<token>     tokens;

    table                              head;
    std::vector<std::vector<uint32_t>> prev;

    compact::vector<uint64_t, 40> dp_cost;
    std::vector<uint32_t>         dp_from;

public:
    mpo_encoder();

    std::pair<std::byte *, size_t &> for_loading();

private:
    static constexpr uint32_t literal_cost = estimators::literal_cost<uint32_t>;

    static inline uint32_t estimate_cost(uint32_t dist, uint32_t len) {
        return estimators::cost(dist, len);
    }

    void process(auto future_limit, const auto NONE, auto i, auto &best_match_len,
                 auto &best_match_pos);

public:
    std::vector<token> encode();
};
