#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "hashes.h"

class prefix_table {
    std::unordered_map<uint64_t, std::vector<uint32_t>> map;

    constexpr auto hash(auto begin, auto end) const noexcept {
        return hashes::murmur3(begin, end);
    }

public:
    inline void clear() {
        map.clear();
    }

    inline void insert(auto begin, auto end, uint32_t relative_pos) {
        map[hash(begin, end)].push_back(relative_pos);
    }

    inline const std::vector<uint32_t> *find(auto begin, auto end) const {
        auto it = map.find(hash(begin, end));
        if (it != map.end()) {
            return &it->second;
        } else {
            return nullptr;
        }
    }
};

