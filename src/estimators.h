#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstdarg>
#include <cstdint>
#include <map>
#include <vector>

#include "bins.h"

namespace estimators {
struct state {
    uint32_t dist_cache[3];
    state(uint32_t dist0, uint32_t dist1, uint32_t dist2) {
        dist_cache[0] = dist0;
        dist_cache[1] = dist1;
        dist_cache[2] = dist2;
    }
    state() : state(0, 0, 0) {
    }
};

struct smart {
    std::vector<double> controls_table;
    std::vector<double> lengths_table;
    std::vector<double> literals_table;
    std::vector<double> ctx_table;

    void clear() {
        controls_table.clear();
        lengths_table.clear();
        literals_table.clear();
        ctx_table.clear();
    }

    void fill(std::vector<double> &table, const auto &values) {
        if (values.empty()) {
            table.assign(256, 1e99);
            return;
        }

        std::map<uint32_t, uint32_t> map;
        uint64_t                     total = 0;
        uint32_t                     max_val = 0;

        for (const auto &v : values) {
            uint32_t val = static_cast<uint32_t>(v);
            map[val]++;
            total++;
            if (val > max_val) {
                max_val = val;
            }
        }

        size_t table_size = static_cast<size_t>(max_val) + 1;
        table.assign(table_size, 1e99);

        std::vector<std::pair<uint32_t, uint32_t>> pairs;
        pairs.reserve(map.size());
        for (const auto &[val, cnt] : map) {
            pairs.push_back({val, cnt});
        }

        for (size_t i = 0; i < table_size; ++i) {
            uint32_t value = static_cast<uint32_t>(i);

            auto it =
                std::lower_bound(pairs.begin(), pairs.end(), value,
                                 [](const std::pair<uint32_t, uint32_t> &element,
                                    uint32_t val) { return element.first < val; });

            if (it != pairs.end() && it->first == value) {
                table[i] = -std::log2(static_cast<double>(it->second) / total);
                continue;
            }

            if (it == pairs.begin()) {
                table[i] =
                    -std::log2(static_cast<double>(pairs.front().second) / total);
                continue;
            }

            if (it == pairs.end()) {
                table[i] =
                    -std::log2(static_cast<double>(pairs.back().second) / total);
                continue;
            }

            auto     it_prev = std::prev(it);
            uint32_t a = it_prev->first;
            uint32_t count_a = it_prev->second;
            uint32_t b = it->first;
            uint32_t count_b = it->second;

            uint64_t weighted_sum = static_cast<uint64_t>(count_a) * (b - value) +
                                    static_cast<uint64_t>(count_b) * (value - a);
            uint32_t total_distance = b - a;

            uint32_t estimated_count = static_cast<uint32_t>(
                (weighted_sum + (total_distance / 2)) / total_distance);

            if (estimated_count == 0) {
                table[i] = 1e99;
            } else {
                table[i] = -std::log2(static_cast<double>(estimated_count) / total);
            }
        }
    }

    inline constexpr double control_cost(uint64_t control) {
        if (controls_table.empty()) {
            return 1;
        }
        if (control >= controls_table.size())
            return 1e99;
        return controls_table[control];
    }

    inline constexpr double literal_cost(uint64_t literal) {
        if (literals_table.empty()) {
            return 7;
        }
        if (literal >= literals_table.size())
            return control_cost(0) + 1e99;
        return control_cost(0) + literals_table[literal];
    }

    inline constexpr double match_cost(uint64_t dist, uint64_t len, const state &s) {
        if (literals_table.empty()) {
            using T = uint64_t;
            constexpr T full_bits = 8 * sizeof(T);
            T           len_cost = full_bits - std::countl_zero(len);

            if (s.dist_cache[0] == dist || s.dist_cache[1] == dist ||
                s.dist_cache[2] == dist) {
                return control_cost(1) + len_cost;
            } else {
                auto           info = dist_bins::get(dist);
                constexpr auto ctx_cost =
                    full_bits -
                    std::countl_zero(static_cast<T>(dist_bins::get(1e9).ctx));
                return control_cost(4) + ctx_cost + info.extra_bits + len_cost;
            }
        }
        double len_cost = (len < lengths_table.size()) ? lengths_table[len] : 1e99;

        if (s.dist_cache[0] == dist) {
            return control_cost(1) + len_cost;
        } else if (s.dist_cache[1] == dist) {
            return control_cost(2) + len_cost;
        } else if (s.dist_cache[2] == dist) {
            return control_cost(3) + len_cost;
        } else {
            auto   info = dist_bins::get(dist);
            double ctx_cost =
                (info.ctx < ctx_table.size()) ? ctx_table[info.ctx] : 1e99;
            return control_cost(4) + ctx_cost + info.extra_bits + len_cost;
        }
    }
};
}  // namespace estimators
