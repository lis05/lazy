#pragma once
#include <algorithm>
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
#include "token.h"
#include "worker_pool.h"

class div_encoder {
    size_t                 bytes_loaded;
    std::vector<std::byte> data;
    std::vector<token>     tokens;
    std::vector<uint32_t>  head;
    std::vector<uint32_t>  prev;

    std::vector<std::vector<uint32_t>> hashpos;  // for each hash contains the list
                                                 // of positions where that hash
                                                 // exists (in increasing order)
    std::vector<uint32_t> worker_len, worker_pos;
    worker_pool           pool;

public:
    div_encoder();

    std::pair<std::byte *, size_t &> for_loading();

private:
    static constexpr uint32_t literal_cost = 8;

    static inline uint32_t estimate_cost(uint32_t dist, uint32_t len) {
        return literal_cost + (32 - std::countl_zero(dist)) +
               (32 - std::countl_zero(len));
    }

    void process(auto future_limit, const auto NONE, auto i, auto &best_match_len,
                 auto &best_match_pos);

public:
    // can only run once.
    std::vector<token> encode();
};
