#include "encoder.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <latch>
#include <mutex>
#include <thread>

#include "bins.h"
#include "split.h"
#include "table.h"
#include "worker_pool.h"

struct saved_hashchains {
    uint32_t                           data_checksum;
    std::vector<uint32_t>              prefix_lengths;
    std::vector<std::vector<uint32_t>> chains;

    friend auto &operator<<(auto &out, const saved_hashchains &h) {
        out.write(reinterpret_cast<const char *>(&h.data_checksum),
                  sizeof(h.data_checksum));

        uint64_t pl_size = static_cast<uint64_t>(h.prefix_lengths.size());
        out.write(reinterpret_cast<const char *>(&pl_size), sizeof(pl_size));
        out.write(reinterpret_cast<const char *>(h.prefix_lengths.data()),
                  pl_size * sizeof(uint32_t));

        uint64_t chains_size = static_cast<uint64_t>(h.chains.size());
        out.write(reinterpret_cast<const char *>(&chains_size), sizeof(chains_size));
        for (const auto &p : h.chains) {
            uint64_t p_size = static_cast<uint64_t>(p.size());
            out.write(reinterpret_cast<const char *>(&p_size), sizeof(p_size));
            out.write(reinterpret_cast<const char *>(p.data()),
                      p_size * sizeof(uint32_t));
        }
        return out;
    }

    friend auto &operator>>(auto &in, saved_hashchains &h) {
        in.read(reinterpret_cast<char *>(&h.data_checksum), sizeof(h.data_checksum));

        uint64_t pl_size = 0;
        in.read(reinterpret_cast<char *>(&pl_size), sizeof(pl_size));
        h.prefix_lengths.resize(pl_size);
        in.read(reinterpret_cast<char *>(h.prefix_lengths.data()),
                pl_size * sizeof(uint32_t));

        uint64_t chains_size = 0;
        in.read(reinterpret_cast<char *>(&chains_size), sizeof(chains_size));
        h.chains.resize(chains_size);
        for (auto &p : h.chains) {
            uint64_t p_size = 0;
            in.read(reinterpret_cast<char *>(&p_size), sizeof(p_size));
            p.resize(p_size);
            in.read(reinterpret_cast<char *>(p.data()), p_size * sizeof(uint32_t));
        }
        return in;
    }
};

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

static bool load_hashchains(encoder &enc) {
    if (config::load_hashchains.empty()) {
        return false;
    }

    config::start_action(
        std::format("Loading hashchains from {}", config::load_hashchains));
    std::ifstream in(config::load_hashchains, std::ios::binary);
    if (!in) {
        return false;
    }

    saved_hashchains h;
    in >> h;

    if (in.fail() && !in.eof()) {
        throw std::runtime_error("load_hashchains: File read error");
    }

    uint32_t current_checksum =
        static_cast<uint32_t>(hashes::hashn(enc.data.data(), enc.bytes_loaded));
    if (h.data_checksum != current_checksum ||
        !std::ranges::equal(h.prefix_lengths, config::prefix_lengths)) {
        return false;
    }

    enc->chains = std::move(h.chains);
    return true;
}

void sync_hashchains(const encoder &enc) {
    if (config::load_hashchains.empty()) {
        return;
    }

    uint32_t current_checksum =
        static_cast<uint32_t>(hashes::hashn(enc.data.data(), enc.bytes_loaded));

    std::ifstream in(config::load_hashchains, std::ios::binary);
    if (in) {
        uint32_t read_checksum = 0;
        in.read(reinterpret_cast<char *>(&read_checksum), sizeof(read_checksum));

        uint64_t pl_size = 0;
        in.read(reinterpret_cast<char *>(&pl_size), sizeof(pl_size));
        std::vector<uint32_t> read_prefix_lengths(pl_size);
        in.read(reinterpret_cast<char *>(read_prefix_lengths.data()),
                pl_size * sizeof(uint32_t));

        if (!in.fail() && read_checksum == current_checksum &&
            std::ranges::equal(read_prefix_lengths, config::prefix_lengths)) {
            return;
        }
    }
    in.close();

    config::start_action(
        std::format("Saving hashchains to {}", config::load_hashchains));
    std::ofstream out(config::load_hashchains, std::ios::binary);
    if (!out) {
        throw std::runtime_error("sync_hashchains: Failed to open file for writing");
    }

    out.write(reinterpret_cast<const char *>(&current_checksum),
              sizeof(current_checksum));

    uint64_t pl_size = static_cast<uint64_t>(config::prefix_lengths.size());
    out.write(reinterpret_cast<const char *>(&pl_size), sizeof(pl_size));
    out.write(reinterpret_cast<const char *>(config::prefix_lengths.data()),
              pl_size * sizeof(uint32_t));

    uint64_t chains_size = static_cast<uint64_t>(enc.chains.size());
    out.write(reinterpret_cast<const char *>(&chains_size), sizeof(chains_size));
    for (const auto &p : enc.chains) {
        uint64_t p_size = static_cast<uint64_t>(p.size());
        out.write(reinterpret_cast<const char *>(&p_size), sizeof(p_size));
        out.write(reinterpret_cast<const char *>(p.data()),
                  p_size * sizeof(uint32_t));
    }

    if (!out) {
        throw std::runtime_error("sync_hashchains: Failed to write data");
    }
}

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

    uint32_t current_checksum =
        static_cast<uint32_t>(enc.hashes::hashn(enc.data.data(), enc.bytes_loaded));
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

    uint32_t current_checksum =
        static_cast<uint32_t>(enc.hashes::hashn(enc.data.data(), enc.bytes_loaded));

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
    bytes_loaded = count;
    data.resize(bytes_loaded + 256);
    std::memcpy(data.data(), from, bytes_loaded);
}

void encoder::reset_for_next_pass(uint32_t pass) {
    if (pass + 1 == config::passes) {
        return;
    }

    using T = decltype(chains);
    T chains_saved;
    chains_saved.swap(chains);

    bool are_tokens_available_saved = are_tokens_available;

    *this = encoder();
    this->chains.swap(chains_saved);
    this->are_tokens_available = are_tokens_available_saved;
}

[[gnu::always_inline]] static inline void process(
    uint32_t i, const std::byte *data_ptr, const std::vector<uint32_t> *chains_ptr,
    const uint32_t *prefix_lengths_ptr, const uint32_t *max_matches_ptr,
    uint32_t *sheet_ptr) {
    const int max_prefixes = config::prefix_lengths.size();
    uint32_t  future_limit = config::max_match_length;

    for (int prefix = max_prefixes - 1; prefix >= 0; prefix--) {
        const uint32_t prefix_len = prefix_lengths_ptr[prefix];

        if (prefix + 1 != max_prefixes &&
            future_limit > prefix_lengths_ptr[prefix + 1]) {
            future_limit = prefix_lengths_ptr[prefix + 1] - 1;
        }

        const uint32_t *chain_ptr = chains_ptr[prefix].data();

        uint32_t count = max_matches_ptr[prefix];
        for (uint32_t pos = chain_ptr[i]; pos != NONE32 && --count > 0;
             pos = chain_ptr[pos]) {
            uint32_t match_len =
                strmatch::match(future_limit, data_ptr + pos, data_ptr + i) & 0xFF;
            if (sheet_ptr[match_len] == NONE32 || sheet_ptr[match_len] < pos) {
                sheet_ptr[match_len] = pos;
            }
            if (match_len >= future_limit) {
                break;
            }
        }
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

    worker_pool pool(config::threads);

    if (chains.empty() && !load_hashchains(*this)) {
        hashes.resize(bytes_loaded);
        chains.resize(config::prefix_lengths.size());

        __gnu_pbds::gp_hash_table<uint32_t, uint32_t> head_gp;
        table                                         head_t(1);
        if (config::hash_bits != 0) {
            head_t = table(config::hash_bits);
        }

        for (int prefix = config::prefix_lengths.size() - 1; prefix >= 0; prefix--) {
            const auto prefix_len = config::prefix_lengths[prefix];
            config::start_action(
                std::format("Calculating hashes for prefix_len = {}", prefix_len));

            auto blocks =
                split_range(bytes_loaded, config::threads, 1, bytes_loaded);
            std::latch finished(blocks.size());
            uint32_t  *hashes_ptr = hashes.data();

            for (auto [start, end] : blocks) {
                pool.enqueue([&, start, end]() mutable {
                    while (start <= end) {
                        hashes_ptr[start] =
                            hashes::hashn(data_ptr + start, prefix_len);
                        start++;
                    }
                    finished.count_down();
                });
            }

            finished.wait();

            config::print_message(
                std::format("Calculating chain for prefix_len = {}", prefix_len));

            chains[prefix].resize(bytes_loaded);
            uint32_t chain_ptr = chains[prefix].data();
            if (config::hash_bits == 0) {
                for (uint32_t i = 0; i < bytes_loaded; i++) {
                    auto it = head_gp.find(hashes_ptr[i]);
                    if (it == head_gp.end()) {
                        chain_ptr[i] = NONE32;
                    } else {
                        chain_ptr[i] = it->second;
                    }
                    head_gp[h] = i;
                }
                head_gp.clear();
            } else {
                for (uint32_t i = 0; i < bytes_loaded; i++) {
                    auto h = hashes_ptr[i];
                    pr_ptr[i] = head_t.get(h);
                    head_t.insert(h, i);
                }
                head_t.clear();
            }
        }
        sync_hashchains(*this);
    }

    {
        using T = decltype(hashes);
        T{}.swap(hashes);
    }

    // write_stats_hashes(est);

    config::start_action_with_counter(std::format("Compressing divisions"));

    auto blocks = split_range(bytes_loaded, config::divisions, 1, bytes_loaded);
    std::latch finished(blocks.size());

    config::max_counter = blocks.size();
    config::counter = 0;

    std::vector<std::vector<token>>             dp_tokens(blocks.size());
    std::vector<std::vector<uint32_t>>          dp_sheet(blocks.size());
    std::vector<std::vector<double>>            dp_cost(blocks.size());
    std::vector<std::vector<uint32_t>>          dp_from(blocks.size());
    std::vector<std::vector<uint32_t>>          dp_pos(blocks.size());
    std::vector<std::vector<estimators::state>> dp_state(blocks.size());

    const std::vector<uint32_t> *chains_ptr = chains.data();
    const uint32_t              *prefix_lengths_ptr = config::prefix_lengths.data();
    const uint32_t              *max_matches_ptr = config::max_matches.data();

    uint32_t block_index = 0;
    for (auto [start, end] : blocks) {
        pool.enqueue([&, start, end, block_index]() mutable {
            end++;  // exclusive end
            uint32_t block_size = end - start;

            dp_sheet[block_index].resize(config::max_match_length + 1, NONE32);
            dp_cost[block_index].resize(block_size + 1, INF64);
            dp_from[block_index].resize(block_size + 1, NONE32);
            dp_pos[block_index].resize(block_size + 1, NONE32);
            dp_state[block_index].resize(block_size + 1,
                                         estimators::state(NONE32, NONE32, NONE32));

            uint32_t          *dp_sheet_ptr = dp_sheet[block_index].data();
            double            *dp_cost_ptr = dp_cost[block_index].data();
            uint32_t          *dp_from_ptr = dp_from[block_index].data();
            uint32_t          *dp_pos_ptr = dp_pos[block_index].data();
            estimators::state *dp_state_ptr = dp_state[block_index].data();

            uint32_t i = start;

            dp_cost_ptr[i - start] = 0;
            dp_from_ptr[i - start] = start;
            dp_pos_ptr[i - start] = NONE32;
            while (i < end) {
                process(i, data_ptr, chains_ptr, prefix_lengths_ptr, max_matches_ptr,
                        sheet_ptr);

                for (uint32_t match_len = config::max_match_length;
                     match_len >= config::min_match_length; match_len--) {
                    if (match_len != config::max_match_length &&
                        sheet_ptr[match_len + 1] != NONE32 &&
                        (sheet_ptr[match_len] == NONE32 ||
                         sheet_ptr[match_len] < sheet_ptr[match_len + 1])) {
                        sheet_ptr[match_len] = sheet_ptr[match_len + 1];
                    }

                    if (sheet_ptr[match_len] == NONE32) {
                        continue;
                    }

                    double edge_cost = est.match_cost(i - sheet_ptr[match_len],
                                                      match_len, states[i]);
                    // edge
                    if (i + match_len < end) {
                        double new_cost = dp_cost_ptr[i - start] + edge_cost;
                        if (dp_cost_ptr[i + match_len - start] > new_cost) {
                            dp_cost_ptr[i + match_len - start] = new_cost;
                            dp_from_ptr[i + match_len - start] = i;
                            dp_pos_ptr[i + match_len - start] = sheet_ptr[match_len];
                            const auto &src = states[i - start];
                            auto       &s = states[i - start + match_len];
                            s = estimators::state{
                                static_cast<uint32_t>(i - sheet_ptr[match_len]),
                                src.dist_cache[0], src.dist_cache[1]};
                        }
                    }
                }

                // literal
                if (i + 1 <= end) {
                    double new_cost =
                        dp_cost_ptr[i - start] +
                        est.literal_cost(static_cast<uint64_t>(data_ptr[i]));
                    if (dp_cost_ptr[i + 1 - start] > new_cost) {
                        dp_cost_ptr[i + 1 - start] = new_cost;
                        dp_from_ptr[i + 1 - start] = i;
                        const auto &src = states[i - start];
                        auto       &s = states[i + 1 - start];
                        s = src;
                    }
                }

                i++;
            }

            i = end;
            while (i > start) {
                if (dp_cost_ptr[i - start] == INF64) {
                    throw std::runtime_error(
                        std::format("Optimal encoder has failed at {}", i));
                }

                uint32_t came_from = dp_from_ptr[i - start];
                if (came_from + 1 == i) {
                    // literal
                    dp_tokes_ptr.push_back(data_ptr[came_from]);
                    i = came_from;
                    continue;
                }

                uint32_t best_match_len = i - came_from;
                uint32_t best_match_pos = dp_pos_ptr[i - start];
                if (best_match_len == 1) {
                    throw std::runtime_error(std::format(
                        "Optimal encoder has failed miserably at {} with "
                        "best_match_len={}",
                        i, best_match_len));
                }

                dp_tokens_ptr.push_back(
                    match{came_from - best_match_pos, best_match_len});
                i = came_from;
            }

            if (i != start) {
                throw std::runtime_error(std::format("Optimal encoder has failed"));
            }

            {
                using T = decltype(dp_sheet);
                T{}.swap(dp_sheet);
            }
            {
                using T = decltype(dp_cost);
                T{}.swap(dp_cost);
            }
            {
                using T = decltype(dp_from);
                T{}.swap(dp_from);
            }
            {
                using T = decltype(dp_pos);
                T{}.swap(dp_pos);
            }
            {
                using T = decltype(dp_state);
                T{}.swap(dp_state);
            }

            finished.count_down();
            config::counter++;
        });
        block_index++;
    }

    finished.wait();

    config::start_action("Finalizing tokens");
    for (auto &e : dp_tokens) {
        std::reverse(e.begin(), e.end());
        for (auto &ee : e) {
            tokens.push_back(ee);
        }

        using T = std::decay_t<decltype(e)>;
        T{}.swap(e);
    }

    if (pass + 1 == config::passes) {
        sync_tokens();
    }

    // write_stats_tokens(est);

    if (pass + 1 != config::passes) {
        est.clear();

        std::vector<uint8_t> controls, lengths, literals, ctx;
        estimators::state    s{0, 0, 0};

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
