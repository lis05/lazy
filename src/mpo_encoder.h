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
#include "hashes.h"
#include "strmatch.h"
#include "table.h"
#include "token.h"

class mpo_encoder {
    size_t                 bytes_loaded;
    std::vector<std::byte> data;
    std::vector<token>     tokens;

    std::vector<estimators::better::state> states;

    std::vector<uint32_t> hashes;
    __gnu_pbds::gp_hash_table<uint32_t, uint32_t> head_gp;
    table                                         head;
    std::vector<std::vector<uint32_t>>            prev;

    std::vector<std::vector<uint32_t>> subblock;

    std::vector<uint64_t> dp_cost;
    std::vector<uint32_t> dp_from;
    std::vector<uint32_t> dp_pos;

public:
    mpo_encoder();

    std::pair<std::byte *, size_t &> for_loading();

private:
    void process(auto future_limit, const auto NONE, auto i, auto subblock_start);

    void write_stats_hashes();
    void write_stats_tokens();

public:
    std::vector<token> encode();
};
