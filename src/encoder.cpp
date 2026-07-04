#include "encoder.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <latch>
#include <mutex>
#include <thread>

#include "bins.h"
#include "hashes.h"
#include "split.h"
#include "strmatch.h"
#include "worker_pool.h"

constexpr uint32_t padding = 32;

struct saved_tokens {
    uint32_t           data_checksum;
    std::vector<token> tokens;

    friend auto &operator<<(auto &out, const saved_tokens &t) {
        out.write(reinterpret_cast<const char *>(&t.data_checksum),
                  sizeof(t.data_checksum));
        uint64_t t_size = static_cast<uint64_t>(t.tokens.size());
        out.write(reinterpret_cast<const char *>(&t_size), sizeof(t_size));
        for (const auto &tok : t.tokens) {
            if (std::holds_alternative<std::byte>(tok)) {
                uint8_t type = 0;
                out.write(reinterpret_cast<const char *>(&type), 1);
                auto b = std::get<std::byte>(tok);
                out.write(reinterpret_cast<const char *>(&b), 1);
            } else {
                uint8_t type = 1;
                out.write(reinterpret_cast<const char *>(&type), 1);
                auto m = std::get<match>(tok);
                out.write(reinterpret_cast<const char *>(&m.distance),
                          sizeof(m.distance));
                out.write(reinterpret_cast<const char *>(&m.length),
                          sizeof(m.length));
            }
        }
        return out;
    }

    friend auto &operator>>(auto &in, saved_tokens &t) {
        in.read(reinterpret_cast<char *>(&t.data_checksum), sizeof(t.data_checksum));
        uint64_t t_size = 0;
        in.read(reinterpret_cast<char *>(&t_size), sizeof(t_size));
        t.tokens.resize(t_size);
        for (uint64_t i = 0; i < t_size; i++) {
            uint8_t type = 0;
            in.read(reinterpret_cast<char *>(&type), 1);
            if (type == 0) {
                std::byte b;
                in.read(reinterpret_cast<char *>(&b), 1);
                t.tokens[i] = b;
            } else {
                match m;
                in.read(reinterpret_cast<char *>(&m.distance), sizeof(m.distance));
                in.read(reinterpret_cast<char *>(&m.length), sizeof(m.length));
                t.tokens[i] = m;
            }
        }
        return in;
    }
};

bool load_tokens(encoder &enc) {
    if (config::load_tokens.empty()) {
        return false;
    }

    config::start_action(std::format("Loading tokens from {}", config::load_tokens));
    std::ifstream in(config::load_tokens, std::ios::binary);
    if (!in) {
        return false;
    }

    saved_tokens t;
    in >> t;

    if (in.fail() && !in.eof()) {
        throw std::runtime_error("load_tokens: File read error");
    }

    uint32_t current_checksum = static_cast<uint32_t>(
        hashes::hashn(enc.data.data(), enc.bytes_loaded + padding));
    if (t.data_checksum != current_checksum) {
        return false;
    }

    enc.tokens = std::move(t.tokens);
    return true;
}

void sync_tokens(const encoder &enc) {
    if (config::load_tokens.empty()) {
        return;
    }

    uint32_t current_checksum = static_cast<uint32_t>(
        hashes::hashn(enc.data.data(), enc.bytes_loaded + padding));

    config::start_action(std::format("Saving tokens to {}", config::load_tokens));
    std::ofstream out(config::load_tokens, std::ios::binary);
    if (!out) {
        throw std::runtime_error("sync_tokens: Failed to open file for writing");
    }

    saved_tokens t{current_checksum, enc.tokens};
    out << t;

    if (!out) {
        throw std::runtime_error("sync_tokens: Failed to write data");
    }
}

void encoder::load(const std::byte *from, uint32_t count) {
    if (count < 256) {
        throw std::runtime_error("File is incompressible");
    }
    bytes_loaded = count - padding;
    data.resize(count + 256);  // + 256 since process() ignores file size
    std::memcpy(data.data(), from, count);
}

void encoder::reset_for_next_pass(uint32_t pass) {
    if (pass + 1 == config::passes) {
        return;
    }

    using T = decltype(chains);
    T chains_saved;
    chains_saved.swap(chains);

    using U = decltype(data);
    U data_saved;
    data_saved.swap(data);

    using V = decltype(tables);
    V tables_saved;
    tables_saved.swap(tables);

    uint32_t bytes_loaded_saved = bytes_loaded;

    bool are_tokens_available_saved = are_tokens_available;

    *this = encoder();
    this->chains.swap(chains_saved);
    this->tables.swap(tables_saved);
    this->are_tokens_available = are_tokens_available_saved;
    this->data.swap(data_saved);
    this->bytes_loaded = bytes_loaded_saved;
}

// clang-format off
// calculates sheet for i
[[gnu::always_inline]]
static inline void process(
    uint32_t i,                                  // current position
    const std::byte *data_ptr,
    uint32_t chains_end,                         // where chains end (one after)
    const std::vector<uint32_t> *chains_ptr,
    uint32_t full_chains,                        // how many full chains out of all
    const uint32_t *prefix_lengths_ptr,
    const uint32_t *chain_masks,
    const uint32_t *max_matches_ptr,
    const uint32_t *depth_limit_ptr,             // 1 + mask
    uint32_t num_chains,                         // how many prefix lengths
    uint32_t *sheet_ptr
) {
    // NOTE: --count is intentionally treating 0 as max depth

    // clang-format on
    uint32_t future_limit = config::max_match_length;

    // full chains first
    uint32_t chain = 0;
    for (; chain < full_chains; chain++) {
        uint32_t prefix_len = prefix_lengths_ptr[chain];

        if (chain != 0 && future_limit > prefix_lengths_ptr[chain - 1]) {
            future_limit = prefix_lengths_ptr[chain - 1] - 1;
        }

        const uint32_t *chain_ptr = chains_ptr[chain].data();

        uint32_t count = max_matches_ptr[chain];
        uint32_t depth = depth_limit_ptr[chain];
        for (uint32_t pos = chain_ptr[i];
             --count > 0 && pos != NONE32 && i - pos <= depth;
             pos = chain_ptr[pos]) {
            uint32_t match_len =
                strmatch::match(future_limit, data_ptr + pos, data_ptr + i) &
                config::max_match_length;
            if (sheet_ptr[match_len] == NONE32 || sheet_ptr[match_len] < pos) {
                sheet_ptr[match_len] = pos;
            }
            if (match_len >= future_limit) {
                break;
            }
        }
    }

    // partial chains second
    for (; chain < num_chains; chain++) {
        uint32_t prefix_len = prefix_lengths_ptr[chain];

        if (chain != 0 && future_limit > prefix_lengths_ptr[chain - 1]) {
            future_limit = prefix_lengths_ptr[chain - 1] - 1;
        }

        const uint32_t *chain_ptr = chains_ptr[chain].data();
        uint32_t        chain_mask = chain_masks[chain];

        uint32_t count = max_matches_ptr[chain];
        uint32_t depth = depth_limit_ptr[chain];
        if (chains_end - i > depth) {
            continue;
        }
        uint32_t step = chain_ptr[i & chain_mask];
        if (step == NONE32 || step > i) {
            continue;
        }
        uint32_t pos = i - step;
        while (--count > 0 && chains_end - pos <= depth) {
            uint32_t match_len =
                strmatch::match(future_limit, data_ptr + pos, data_ptr + i) &
                config::max_match_length;
            if (sheet_ptr[match_len] == NONE32 || sheet_ptr[match_len] < pos) {
                sheet_ptr[match_len] = pos;
            }
            if (match_len >= future_limit) {
                break;
            }

            uint32_t step = chain_ptr[pos & chain_mask];
            if (step == NONE32 || step > pos) {
                break;
            }
            pos -= step;
        }
    }
}

// clang-format off
// advances all partial chains to i (by one)
[[gnu::always_inline]]
static inline void advance_partial(
    uint32_t i,                                  // current position
    const std::byte *data_ptr,
    std::vector<uint32_t> *chains_ptr,
    uint32_t full_chains,                        // how many full chains out of all
    const uint32_t *prefix_lengths_ptr,
    const uint32_t *chain_masks,
    uint32_t num_chains,                         // how many prefix lengths
    std::vector<uint32_t> *tables_ptr,
    const uint32_t *table_masks
) {
    // clang-format on
    for (uint32_t chain = full_chains; chain < num_chains; chain++) {
        uint32_t *chain_ptr = chains_ptr[chain].data();
        uint32_t *table_ptr = tables_ptr[chain].data();
        uint32_t  table_mask = table_masks[chain];
        uint32_t  prefix_len = prefix_lengths_ptr[chain];
        uint32_t  hash = hashes::hashn(data_ptr + i, prefix_len);
        uint32_t  prev = table_ptr[hash & table_mask];
        chain_ptr[i & chain_masks[chain]] = i - prev;
        table_ptr[hash & table_mask] = i;
    }
}

std::vector<token> encoder::encode(uint32_t pass, estimators::smart &est) {
    if (!config::load_tokens.empty()) {
        if (pass == 0) {
            if (load_tokens(*this)) {
                are_tokens_available = true;
                return tokens;
            }
        } else if (pass + 1 == config::passes) {
            if (load_tokens(*this)) {
                return tokens;
            }
        } else if (are_tokens_available) {
            return {};
        }
    }

    const std::byte *data_ptr = data.data();
    uint32_t         num_chains = config::prefix_lengths.size();
    const uint32_t  *prefix_lengths_ptr = config::prefix_lengths.data();
    const uint32_t  *max_matches_ptr = config::max_matches.data();
    const uint32_t  *hash_bits_ptr = config::hash_bits.data();

    std::vector<uint32_t> _depth_limit;
    for (uint32_t l : config::depth_limit_log) {
        _depth_limit.push_back(uint64_t{1} << l);
    }
    const uint32_t *depth_limit_ptr = _depth_limit.data();

    std::vector<uint32_t> _chain_masks;
    for (auto l : _depth_limit) {
        _chain_masks.push_back(l - 1);
    }
    const uint32_t *chain_masks_ptr = _chain_masks.data();

    std::vector<uint32_t> _table_masks;
    for (auto l : config::hash_bits) {
        _table_masks.push_back((uint64_t{1} << l) - 1);
    }
    const uint32_t *table_masks_ptr = _table_masks.data();

    uint32_t full_chains = 0;
    while (full_chains < num_chains &&
           depth_limit_ptr[full_chains] >= bytes_loaded) {
        full_chains++;
    }

    worker_pool pool(config::threads);

    if (pass == 0) {
        chains.resize(num_chains);
        tables.resize(num_chains);

        // calculate full chains
        for (uint32_t chain = 0; chain < full_chains; chain++) {
            auto blocks = split_range(bytes_loaded, config::threads * config::blocks,
                                      1, bytes_loaded);
            std::vector<uint32_t> table(table_masks_ptr[chain] + 1, NONE32);
            uint32_t             *table_ptr = table.data();
            uint32_t              table_mask = table_masks_ptr[chain];

            uint32_t prefix_len = config::prefix_lengths[chain];

            config::start_action(std::format(
                "Calculating full chain for prefix_len = {}", prefix_len));
            chains[chain].resize(bytes_loaded);
            uint32_t *chain_ptr = chains[chain].data();

            for (auto [start, end] : blocks) {
                end++;  // exclusive end
                uint32_t block_size = end - start;

                std::vector<uint32_t> hashes(block_size);
                uint32_t             *hashes_ptr = hashes.data();

                auto subblocks =
                    split_range(block_size, config::threads, 1, block_size);
                std::latch finished(subblocks.size());

                for (auto [sub_start, sub_end] : subblocks) {
                    sub_start += start;
                    sub_end += start;
                    pool.enqueue([&, sub_start, sub_end]() mutable {
                        while (sub_start <= sub_end) {
                            hashes_ptr[sub_start - start] =
                                hashes::hashn(data_ptr + sub_start, prefix_len);
                            sub_start++;
                        }
                        finished.count_down();
                    });
                }

                finished.wait();

                for (uint32_t i = start; i < end; i++) {
                    uint32_t h = hashes_ptr[i - start];
                    chain_ptr[i] = table[h & table_mask];
                    table[h & table_mask] = i;
                }
            }
        }

        // resize partial chains and their tables
        for (uint32_t chain = full_chains; chain < num_chains; chain++) {
            chains[chain].resize(depth_limit_ptr[chain], NONE32);
            tables[chain].resize(table_masks_ptr[chain] + 1, NONE32);
        }
    } else {
        // clear partial chains and their tables
        for (uint32_t chain = full_chains; chain < num_chains; chain++) {
            chains[chain].assign(depth_limit_ptr[chain], NONE32);
            tables[chain].assign(table_masks_ptr[chain] + 1, NONE32);
        }
    }

    config::start_action_with_counter(std::format("Compressing data"));
    config::max_counter = bytes_loaded;
    config::counter = 0;

    std::vector<uint32_t> *chains_ptr = chains.data();
    std::vector<uint32_t> *tables_ptr = tables.data();

    auto blocks = split_range(bytes_loaded, config::blocks, 1, bytes_loaded);
    for (auto [start, end] : blocks) {
        end++;  // exclusive end
        uint32_t block_size = end - start;

        for (uint32_t i = start; i < end; i++) {
            advance_partial(i, data_ptr, chains_ptr, full_chains, prefix_lengths_ptr,
                            chain_masks_ptr, num_chains, tables_ptr,
                            table_masks_ptr);
        }

        auto subblocks = split_range(block_size, config::threads, 1, block_size);
        std::latch finished(subblocks.size());

        std::vector<std::vector<token>>             dp_tokens(subblocks.size());
        std::vector<std::vector<uint32_t>>          dp_sheet(subblocks.size());
        std::vector<std::vector<double>>            dp_cost(subblocks.size());
        std::vector<std::vector<uint32_t>>          dp_from(subblocks.size());
        std::vector<std::vector<uint32_t>>          dp_pos(subblocks.size());
        std::vector<std::vector<estimators::state>> dp_state(subblocks.size());

        uint32_t subblock_index = 0;
        uint32_t chains_end = end;
        for (auto [sub_start, sub_end] : subblocks) {
            pool.enqueue([&, sub_start, sub_end, subblock_index]() mutable {
                sub_start += start;
                sub_end += start;
                sub_end++;  // exclusive sub_end
                uint32_t subblock_size = sub_end - sub_start;

                dp_sheet[subblock_index].resize(config::max_match_length + 1,
                                                NONE32);
                dp_cost[subblock_index].resize(subblock_size + 1, INF64);
                dp_from[subblock_index].resize(subblock_size + 1, NONE32);
                dp_pos[subblock_index].resize(subblock_size + 1, NONE32);
                constexpr uint32_t BUF_MASK = 1023;
                dp_state[subblock_index].resize(
                    BUF_MASK + 1, estimators::state(NONE32, NONE32, NONE32));

                uint32_t          *dp_sheet_ptr = dp_sheet[subblock_index].data();
                double            *dp_cost_ptr = dp_cost[subblock_index].data();
                uint32_t          *dp_from_ptr = dp_from[subblock_index].data();
                uint32_t          *dp_pos_ptr = dp_pos[subblock_index].data();
                estimators::state *dp_state_ptr = dp_state[subblock_index].data();

                uint32_t i = sub_start;

                dp_cost_ptr[i - sub_start] = 0;
                dp_from_ptr[i - sub_start] = sub_start;
                dp_pos_ptr[i - sub_start] = NONE32;
                while (i < sub_end) {
                    std::fill(dp_sheet_ptr,
                              dp_sheet_ptr + config::max_match_length + 1, NONE32);
                    process(i, data_ptr, chains_end, chains_ptr, full_chains,
                            prefix_lengths_ptr, chain_masks_ptr, max_matches_ptr,
                            depth_limit_ptr, num_chains, dp_sheet_ptr);

                    for (uint32_t match_len = config::max_match_length;
                         match_len >= config::min_match_length; match_len--) {
                        if (match_len != config::max_match_length &&
                            dp_sheet_ptr[match_len + 1] != NONE32 &&
                            (dp_sheet_ptr[match_len] == NONE32 ||
                             dp_sheet_ptr[match_len] <
                                 dp_sheet_ptr[match_len + 1])) {
                            dp_sheet_ptr[match_len] = dp_sheet_ptr[match_len + 1];
                        }

                        if (dp_sheet_ptr[match_len] == NONE32) {
                            continue;
                        }

                        double edge_cost =
                            est.match_cost(i - dp_sheet_ptr[match_len], match_len,
                                           dp_state_ptr[(i - sub_start) & BUF_MASK]);
                        // edge
                        if (i + match_len <= sub_end) {
                            double new_cost = dp_cost_ptr[i - sub_start] + edge_cost;
                            if (dp_cost_ptr[i + match_len - sub_start] > new_cost) {
                                dp_cost_ptr[i + match_len - sub_start] = new_cost;
                                dp_from_ptr[i + match_len - sub_start] = i;
                                dp_pos_ptr[i + match_len - sub_start] =
                                    dp_sheet_ptr[match_len];
                                const auto &src =
                                    dp_state_ptr[(i - sub_start) & BUF_MASK];
                                auto &s = dp_state_ptr[(i - sub_start + match_len) &
                                                       BUF_MASK];
                                s = estimators::state{
                                    static_cast<uint32_t>(i -
                                                          dp_sheet_ptr[match_len]),
                                    src.dist_cache[0], src.dist_cache[1]};
                            }
                        }
                    }

                    // literal
                    if (i + 1 <= sub_end) {
                        double new_cost =
                            dp_cost_ptr[i - sub_start] +
                            est.literal_cost(static_cast<uint64_t>(data_ptr[i]));
                        if (dp_cost_ptr[i + 1 - sub_start] > new_cost) {
                            dp_cost_ptr[i + 1 - sub_start] = new_cost;
                            dp_from_ptr[i + 1 - sub_start] = i;
                            const auto &src =
                                dp_state_ptr[(i - sub_start) & BUF_MASK];
                            auto &s = dp_state_ptr[(i + 1 - sub_start) & BUF_MASK];
                            s = src;
                        }
                    }

                    i++;
                }

                i = sub_end;
                while (i > sub_start) {
                    if (dp_cost_ptr[i - sub_start] == INF64) {
                        throw std::runtime_error(
                            std::format("Optimal encoder has failed at {}", i));
                    }

                    uint32_t came_from = dp_from_ptr[i - sub_start];
                    if (came_from + 1 == i) {
                        // literal
                        dp_tokens[subblock_index].push_back(data_ptr[came_from]);
                        i = came_from;
                        continue;
                    }

                    uint32_t best_match_len = i - came_from;
                    uint32_t best_match_pos = dp_pos_ptr[i - sub_start];
                    if (best_match_len == 1) {
                        throw std::runtime_error(std::format(
                            "Optimal encoder has failed miserably at {} with "
                            "best_match_len={}",
                            i, best_match_len));
                    }

                    dp_tokens[subblock_index].push_back(
                        match{came_from - best_match_pos, best_match_len});
                    i = came_from;
                }

                if (i != sub_start) {
                    throw std::runtime_error(
                        std::format("Optimal encoder has failed"));
                }

                {
                    using T = std::decay_t<decltype(dp_sheet[subblock_index])>;
                    T{}.swap(dp_sheet[subblock_index]);
                }
                {
                    using T = std::decay_t<decltype(dp_cost[subblock_index])>;
                    T{}.swap(dp_cost[subblock_index]);
                }
                {
                    using T = std::decay_t<decltype(dp_from[subblock_index])>;
                    T{}.swap(dp_from[subblock_index]);
                }
                {
                    using T = std::decay_t<decltype(dp_pos[subblock_index])>;
                    T{}.swap(dp_pos[subblock_index]);
                }
                {
                    using T = std::decay_t<decltype(dp_state[subblock_index])>;
                    T{}.swap(dp_state[subblock_index]);
                }

                finished.count_down();
                config::counter += subblock_size;
            });
            subblock_index++;
        }
        finished.wait();
        for (auto &e : dp_tokens) {
            std::reverse(e.begin(), e.end());
            for (auto &ee : e) {
                tokens.push_back(ee);
            }
        }
    }

    // last 32 are literals so that decoding can be more branchless
    for (uint32_t cnt = 0; cnt < padding; cnt++) {
        tokens.push_back(data_ptr[bytes_loaded + cnt]);
    }

    config::start_action("Verifying");
    uint32_t pos = 0;
    for (const auto &e : tokens) {
        if (std::holds_alternative<std::byte>(e)) {
            std::byte b = std::get<std::byte>(e);
            if (data_ptr[pos] != b) {
                throw std::runtime_error(
                    "Compression failed: invalid stream of tokens: literal "
                    "mismatch "
                    "at " +
                    std::to_string(pos));
            }
            pos++;
        } else {
            match m = std::get<match>(e);
            for (uint32_t cnt = 0; cnt < m.length; cnt++) {
                if (m.distance > pos) {
                    throw std::runtime_error(
                        "Compression failed: invalid stream of tokens: distance "
                        "too "
                        "large at " +
                        std::to_string(pos));
                }
                if (data_ptr[pos - m.distance] != data_ptr[pos]) {
                    throw std::runtime_error(
                        "Compression failed: invalid stream of tokens: invalid "
                        "match byte at " +
                        std::to_string(pos));
                }
                pos++;
            }
        }
    }

    if (pos != bytes_loaded + padding) {
        throw std::runtime_error("Compression failed: invalid stream of tokens");
    }

    if (pass + 1 == config::passes) {
        sync_tokens(*this);
    }

    // write_stats_tokens(est);

    if (pass + 1 != config::passes) {
        est.clear();

        std::vector<uint8_t> controls, lengths, literals, ctx;
        estimators::state    s{NONE32, NONE32, NONE32};

        for (const auto &t : tokens) {
            if (std::holds_alternative<std::byte>(t)) {
                controls.push_back(0);
                literals.push_back(static_cast<uint8_t>(std::get<std::byte>(t)));
            } else {
                const auto [d, l] = std::get<match>(t);
                lengths.push_back(static_cast<uint8_t>(l));

                if (s.dist_cache[0] == d) {
                    controls.push_back(1);
                } else if (s.dist_cache[1] == d) {
                    controls.push_back(2);
                } else if (s.dist_cache[2] == d) {
                    controls.push_back(3);
                } else {
                    controls.push_back(4);
                    auto info = dist_bins::get(d);
                    ctx.push_back(info.ctx);
                }

                s = estimators::state{static_cast<uint32_t>(d), s.dist_cache[0],
                                      s.dist_cache[1]};
            }
        }

        est.fill(est.controls_table, controls);
        est.fill(est.lengths_table, lengths);
        est.fill(est.literals_table, literals);
        est.fill(est.ctx_table, ctx);
    }

    return tokens;
}
