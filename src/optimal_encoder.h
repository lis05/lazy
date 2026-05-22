#pragma once

#include <algorithm>
#include <bit>
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

class optimal_encoder {
    size_t                 bytes_loaded;
    std::vector<std::byte> data;
    std::vector<token>     tokens;
    std::vector<uint32_t>  head;
    std::vector<uint32_t>  prev;

    // DAG: nodes[i] = <cost, where came from, what token>
    struct node {
        uint64_t price;
        uint32_t came_from;
        token    t;
    };
    std::vector<node> nodes;

    inline uint32_t estimate_cost(const token &t) {
        if (std::holds_alternative<std::byte>(t)) {
            return 8;
        } else {
            auto [d, l] = std::get<match>(t);
            return 8 + (32 - std::countl_zero(d)) + (32 - std::countl_zero(l));
        }
    }

    inline void add_edge(const auto &this_node, auto data_ptr, auto nodes_ptr,
                         uint32_t i, uint32_t pos, uint32_t len) {
        token t;
        if (len == 1) [[unlikely]] {
            t = data_ptr[i];
        } else {
            t = match{i - pos, len};
        }

        auto  new_price = this_node.price + estimate_cost(t);
        auto &other = nodes_ptr[i + len];
        if (new_price < other.price) {
            other.price = new_price;
            other.came_from = i;
            other.t = t;
        }
    }

public:
    optimal_encoder()
        : bytes_loaded(0), data(config::block_size), tokens(), head(), prev() {
    }

    inline std::pair<std::byte *, size_t &> for_loading() {
        return {data.data(), bytes_loaded};
    }

public:
    inline const std::vector<token> encode() {
        constexpr auto NONE = std::numeric_limits<uint32_t>::max();
        constexpr auto INF = std::numeric_limits<uint64_t>::max() / 3;

        head.resize(config::total_hashes);
        prev.resize(bytes_loaded);

        std::fill(head.begin(), head.end(), NONE);
        std::fill(prev.begin(), prev.end(), NONE);
        nodes.resize(bytes_loaded + 1, node{.price = INF});
        nodes[0] = node{.price = 0};

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
        auto nodes_ptr = nodes.data();

        for (uint32_t i = 0; i < bytes_loaded; i++) {
            auto &this_node = nodes[i];

            if (this_node.price >= INF) [[unlikely]] {
                throw std::runtime_error(std::format(
                    "Optimal encoder failed at {}. Try greedy encoder or increase "
                    "parameters.",
                    i));
            }

            uint32_t future_limit = std::min(bytes_loaded - i, config::future_limit);

            size_t count = config::max_matches;
            for (uint32_t pos = prev[i];
                 pos != NONE && pos + config::window_size >= i && --count > 0;
                 pos = prev_buf[pos]) {
                uint32_t match_len = strmatch::match_simd_loop(
                    future_limit, data_buf + pos, data_buf + i);

                for (uint32_t len = 1; len <= match_len; len++) {
                    add_edge(this_node, data_buf, nodes_ptr, i, pos, len);
                }
            }

            add_edge(this_node, data_buf, nodes_ptr, i, i + 1, 1);

            config::processed_bytes++;
        }

        uint32_t pos = bytes_loaded;
        while (pos != 0) {
            auto [price, prev_pos, token] = nodes[pos];
            tokens.push_back(token);
            pos = prev_pos;
        }

        std::reverse(tokens.begin(), tokens.end());

        return tokens;
    }
};
