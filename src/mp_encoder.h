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
#include "token.h"

class mp_encoder {
    size_t                                        bytes_loaded;
    std::vector<std::byte>                        data;
    std::vector<token>                            tokens;
    __gnu_pbds::gp_hash_table<uint32_t, uint32_t> head;
    std::vector<std::vector<uint32_t>> prev;  // at index i we have a hash chain
                                              // where each hash is of size sizes[i]
public:
    static constexpr auto sizes = std::to_array<size_t>(
        {3, 8, 16, 32, 64, 128, 256, 1 << 9, 1 << 10, 1 << 11, 1 << 12, 1 << 13,
         1 << 14, 1 << 15, 1 << 16, 1 << 17, 1 << 18, 1 << 19, 1 << 20});

public:
    mp_encoder();

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
