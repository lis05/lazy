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

class encoder {
    size_t                 bytes_loaded;
    std::vector<std::byte> data;
    std::vector<token>     tokens;
    std::vector<uint32_t>  head;
    std::vector<uint32_t>  prev;

public:
    encoder() : bytes_loaded(0), data(config::block_size), tokens(), head(), prev() {
    }

    inline std::pair<std::byte *, size_t &> for_loading() {
        return {data.data(), bytes_loaded};
    }

private:
    static constexpr uint32_t literal_cost = 8;

    inline uint32_t estimate_cost(uint32_t dist, uint32_t len) {
        return literal_cost + (32 - std::countl_zero(dist)) +
               (32 - std::countl_zero(len));
    }

    inline void process(const auto data_buf, const auto prev_buf, auto future_limit,
                        const auto NONE, auto i, auto &best_match_len,
                        auto &best_match_pos) {
        best_match_len = 1;
        best_match_pos = NONE;

        size_t count = config::max_matches;
        for (uint32_t pos = prev[i];
             pos != NONE && pos + config::window_size >= i && --count > 0;
             pos = prev_buf[pos]) {
            uint32_t match_len = strmatch::match_simd_loop(
                future_limit, data_buf + pos, data_buf + i);
            if (match_len > best_match_len) {
                best_match_len = match_len;
                best_match_pos = pos;
            }
        }
    }

public:
    // can only run once.
    inline std::vector<token> encode() {
        constexpr auto NONE = std::numeric_limits<uint32_t>::max();

        head.resize(config::total_hashes(), NONE);
        prev.resize(bytes_loaded, NONE);

        std::fill(head.begin(), head.end(), NONE);
        std::fill(prev.begin(), prev.end(), NONE);

        auto calculate_hashes = [this]() {
            if (config::prefix_size == 3) {
                for (size_t i = 0; i + 2 < bytes_loaded; i++) {
                    auto h = hashes::hash3(data.data() + i);
                    prev[i] = head[h];
                    head[h] = i;
                }
            } else if (config::prefix_size == 2) {
                for (size_t i = 0; i + 1 < bytes_loaded; i++) {
                    auto h = hashes::hash2(data.data() + i);
                    prev[i] = head[h];
                    head[h] = i;
                }
            }
        };
        calculate_hashes();

        auto data_buf = data.data();
        auto prev_buf = prev.data();

        for (uint32_t i = 0; i < bytes_loaded;) {
            auto old_i = i;

            int      best_skip = 0;
            uint32_t best_skip_len = 1;
            uint32_t best_skip_pos = NONE;
            uint32_t best_match_len = 1;
            uint32_t best_match_pos = NONE;

            for (uint32_t skip = 0; skip <= config::lazy_matching; skip++) {
                uint32_t future_limit =
                    std::min(bytes_loaded - i, config::future_limit);
                if (best_match_len == future_limit) {
                    break;
                }

                uint32_t new_best_skip_len = 1;
                uint32_t new_best_skip_pos = NONE;
                uint32_t new_best_match_len = 1;
                uint32_t new_best_match_pos = NONE;

                process(data_buf, prev_buf, future_limit, NONE, i,
                        new_best_match_len, new_best_match_pos);
                if (new_best_match_len == 1) {
                    break;
                }

                // old big match cost
                uint32_t old_cost =
                    (i == old_i || best_match_len == 1)
                        ? literal_cost
                        : estimate_cost(old_i + best_skip - best_match_pos,
                                        best_match_len);

                // old skip cost
                old_cost += best_skip_len == 1 ? literal_cost * best_skip
                                               : estimate_cost(old_i - best_skip_pos,
                                                               best_skip_len);

                // worst case scenario, we fill the rest of the bytes with literals
                old_cost += literal_cost *
                            (new_best_match_len - best_match_len + skip - best_skip);

                // new big match cost
                uint32_t new_cost =
                    estimate_cost(i - new_best_match_pos, new_best_match_len);

                if (skip > 2) {
                    // skip will likely be in the window
                    process(data_buf, prev_buf, skip, NONE, old_i, new_best_skip_len,
                            new_best_skip_pos);

                    if (new_best_skip_len == skip) {
                        // match
                        new_cost += estimate_cost(old_i - new_best_skip_pos,
                                                  new_best_skip_len);
                    } else {
                        // literals
                        new_cost += literal_cost * skip;
                    }
                } else {
                    // literals
                    new_cost += literal_cost * skip;
                }

                if (new_cost < old_cost) {
                    best_skip = skip;
                    best_skip_pos = new_best_skip_pos;
                    best_skip_len = new_best_skip_len;
                    best_match_len = new_best_match_len;
                    best_match_pos = new_best_match_pos;
                }
                i++;
            }

            i = old_i;

            if (best_skip > 0) {
                if (best_skip_len == 1) {
                    while (best_skip-- > 0) {
                        tokens.push_back(data[i++]);
                    }
                } else {
                    auto m = match{i - best_skip_pos, best_skip_len};
                    if (estimate_cost(m.distance, m.length) <=
                        literal_cost * best_skip_len) {
                        tokens.push_back(match{i - best_skip_pos, best_skip_len});
                        i += best_skip;
                    } else {
                        while (best_skip-- > 0) {
                            tokens.push_back(data[i++]);
                        }
                    }
                }
            }

            if (best_match_len == 1) {
                tokens.push_back(data[i]);
            } else {
                auto m = match{i - best_match_pos, best_match_len};
                if (estimate_cost(m.distance, m.length) <=
                    literal_cost * best_match_len) {
                    tokens.push_back(match{i - best_match_pos, best_match_len});
                } else {
                    tokens.push_back(data[i]);
                    best_match_len = 1;
                }
            }

            i += best_match_len;
            config::processed_bytes += i - old_i;
        }
        return tokens;
    }
};
