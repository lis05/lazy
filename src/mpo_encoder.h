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

    std::vector<estimators::state> states;

    std::vector<uint32_t>                         hashes;
    __gnu_pbds::gp_hash_table<uint32_t, uint32_t> head_gp;
    table                                         head;
    std::vector<std::vector<uint32_t>>            prev;

    std::vector<std::vector<double>>   dp_cost;
    std::vector<std::vector<uint32_t>> dp_from;
    std::vector<std::vector<uint32_t>> dp_pos;

public:
    mpo_encoder();

    std::pair<std::byte *, size_t &> for_loading();

    void reset_for_next_pass(uint32_t pass);

private:
    bool load_hashchains();
    void sync_hashchains();
    bool load_tokens();
    void sync_tokens();
    void process(auto future_limit, const auto NONE, auto i, auto *subblock_ptr);

    void write_stats_hashes(estimators::smart &est);
    void write_stats_tokens(estimators::smart &est);

public:
    std::vector<token> encode(uint32_t pass, estimators::smart &est);
};
