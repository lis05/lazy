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

public:
    // can only run once.
    inline const std::vector<token> encode() {
        constexpr auto NONE = std::numeric_limits<uint32_t>::max();

        head.resize(config::total_hashes, NONE);
        prev.resize(bytes_loaded, NONE);

        std::fill(head.begin(), head.end(), NONE);
        std::fill(prev.begin(), prev.end(), NONE);

        auto calculate_hashes = [this]() {
            for (size_t i = 0; i + 2 < bytes_loaded; i++) {
                auto h = hashes::hash3(data.data() + i);
                prev[i] = head[h];
                head[h] = i;
            }
        };

        calculate_hashes();

        auto data_buf = data.data();
        auto prev_buf = prev.data();

        for (uint32_t i = 0; i < bytes_loaded;) {
            auto old_i = i;

            uint32_t best_match_len = 1;
            uint32_t best_match_pos = NONE;

            uint32_t future_limit = std::min(bytes_loaded - i, config::future_limit);

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

            if (best_match_len != 1 && config::lazy_matching &&
                i + best_match_len < bytes_loaded) {
                auto base_match_len = best_match_len;
                auto base_match_pos = best_match_pos;
                i++;

                best_match_len = 1;
                best_match_pos = NONE;

                uint32_t future_limit =
                    std::min(bytes_loaded - i, config::future_limit);

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

                if (best_match_len <= base_match_len) {
                    best_match_len = base_match_len;
                    best_match_pos = base_match_pos;
                    i--;
                } else {
                    tokens.push_back(data[i - 1]);
                }
            }

            if (best_match_len == 1) {
                tokens.push_back(data[i]);
            } else {
                tokens.push_back(match{i - best_match_pos, best_match_len});
            }
            i += best_match_len;

            config::processed_bytes += i - old_i;
        }

        return tokens;
    }
};
