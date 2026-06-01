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
    div_encoder()
        : bytes_loaded(0),
          data(config::block_size),
          tokens(),
          head(),
          prev(),
          hashpos(),
          pool(config::divisions),
          worker_len(config::divisions),
          worker_pos(config::divisions) {
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

    inline void process(auto future_limit, const auto NONE, auto i,
                        auto &best_match_len, auto &best_match_pos) {
        best_match_len = 1;
        best_match_pos = NONE;
        if (i + 2 >= bytes_loaded) {
            return;
        }

        auto  h = hashes::hash3(data.data() + i);
        auto &positions = hashpos[h];
        auto  it = std::lower_bound(positions.begin(), positions.end(), i);
        if (it == positions.end() || *it != i) [[unlikely]] {
            throw std::runtime_error(
                std::format("lower_bound did not find i={} out of {} positions", i,
                            positions.size()));
        }

        auto it_index = it - positions.begin();
        if (it_index == 0) {
            return;  // no previous hashes yet
        }
        auto workers = std::min((long int)config::divisions, it_index / 8);
        if (workers <= 0) {
            workers = 1;
        }

        for (size_t ii = 0; ii < workers; ii++) {
            worker_len[ii] = 1;
            worker_pos[ii] = NONE;
        }
        std::latch finished(workers);

        auto data_buf = data.data();
        auto positions_buf = positions.data();

        for (size_t ii = 0; ii < workers; ii++) {
            pool.enqueue([&, ii]() {
                auto &best_len = worker_len.data()[ii];
                auto &best_pos = worker_pos.data()[ii];

                auto    *ptr = positions_buf + it_index - 1 - ii;
                uint64_t count = config::max_matches;
                for (uint32_t pos = *ptr;
                     pos + config::window_size >= i && --count > 0;
                     pos = *(ptr -= workers)) {
                    uint32_t match_len = strmatch::match_simd_loop(
                        future_limit, data_buf + pos, data_buf + i);

                    if (match_len > best_len) {
                        best_len = match_len;
                        best_pos = pos;
                    }
                    if (ptr - positions_buf < workers) {
                        break;
                    }
                }

                finished.count_down();
            });
        }

        finished.wait();

        for (size_t ii = 0; ii < workers; ii++) {
            if (worker_len[ii] > best_match_len) {
                best_match_len = worker_len[ii];
                best_match_pos = worker_pos[ii];
            } else if (worker_len[ii] == best_match_len &&
                       worker_pos[ii] > best_match_pos) {
                best_match_len = worker_len[ii];
                best_match_pos = worker_pos[ii];
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

        hashpos.resize(config::total_hashes());

        auto calculate_hashes = [this]() {
            for (size_t i = 0; i + 2 < bytes_loaded; i++) {
                auto h = hashes::hash3(data.data() + i);
                prev[i] = head[h];
                head[h] = i;
            }
        };
        calculate_hashes();

        for (size_t h = 0; h < config::total_hashes(); h++) {
            auto i = head[h];
            while (i != NONE) {
                hashpos[h].push_back(i);
                i = prev[i];
            }
            std::reverse(hashpos[h].begin(), hashpos[h].end());
        }

        auto data_buf = data.data();
        auto prev_buf = prev.data();

        for (uint32_t i = 0; i < bytes_loaded;) {
            auto old_i = i;

            bool     is_best_skip_literals = true;
            uint32_t best_skip_len = 0;
            match    best_skip_match;

            bool  best_big_match_exists = false;
            match best_big_match;

            // until there will be at least one byte after skip
            for (uint32_t skip = 0;
                 skip <= config::lazy_matching && i + skip < bytes_loaded; skip++) {
                uint32_t old_total_len =
                    best_skip_len +
                    (best_big_match_exists ? best_big_match.length : 0);
                // old skip
                uint32_t old_cost = is_best_skip_literals
                                        ? literal_cost * best_skip_len
                                        : estimate_cost(best_skip_match.distance,
                                                        best_skip_match.length);
                old_cost += best_big_match_exists
                                ? estimate_cost(best_big_match.distance,
                                                best_big_match.length)
                                : 0;

                bool     is_skip_literals = true;
                uint32_t skip_len = skip;
                match    skip_match;

                if (skip > 2) {
                    uint32_t pos, len;
                    process(skip, NONE, i, len, pos);

                    if (len == skip) {
                        // found a match for skip
                        if (estimate_cost(i - pos, len) < literal_cost * skip) {
                            // we take it
                            is_skip_literals = false;
                            skip_match = match{i - pos, len};
                        }
                    }
                    // otherwise we keep literals
                }

                uint32_t big_len = 1;
                uint32_t big_pos = NONE;
                match    big_match;

                {
                    uint32_t pos, len;
                    uint32_t future_limit =
                        std::min(bytes_loaded - i - skip, config::future_limit);
                    process(future_limit, NONE, i + skip, len, pos);

                    if (pos != NONE) {
                        assert(len > 1);
                        // found a match for big
                        big_match = match{i + skip - pos, len};
                        big_len = len;
                        big_pos = pos;
                    }
                    // otherwise we will stop
                }

                if (big_pos == NONE) {
                    // no big match found, no point in going further
                    break;
                }

                uint32_t new_total_len = skip + big_match.length;

                // we do not take bad option
                if (new_total_len < old_total_len) {
                    continue;
                }

                uint32_t new_cost = 0;
                // include skip
                new_cost += is_skip_literals ? literal_cost * skip
                                             : estimate_cost(skip_match.distance,
                                                             skip_match.length);
                // include big match
                new_cost += estimate_cost(big_match.distance, big_match.length);

                // old_total_len is less than new_total_len so in the worst case we
                // can take more literals in the old option and get the current one,
                // so we include that in old cost
                old_cost += (new_total_len - old_total_len) * literal_cost;

                // MOMENT OF TRUTH.
                if (new_total_len >= old_total_len && new_cost < old_cost) {
                    is_best_skip_literals = is_skip_literals;
                    best_skip_len = skip;
                    best_skip_match = skip_match;
                    best_big_match_exists = true;
                    best_big_match = big_match;
                }
            }

            // if no big match or it is unoptimal
            if (!best_big_match_exists ||
                estimate_cost(best_big_match.distance, best_big_match.length) >=
                    literal_cost * best_big_match.length) {
                // we take 1 literal and continue;
                tokens.push_back(data[i++]);
            } else {
                // take skip
                if (best_skip_len > 0) {
                    if (is_best_skip_literals) {
                        for (uint32_t ii = 0; ii < best_skip_len; ii++) {
                            tokens.push_back(data[i++]);
                        }
                    } else {
                        tokens.push_back(best_skip_match);
                        i += best_skip_len;
                    }
                }

                tokens.push_back(best_big_match);
                i += best_big_match.length;
            }

            config::processed_bytes += i - old_i;
        }
        return tokens;
    }
};
