#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "hashes.h"

class prefix_table {
    std::unordered_map<uint64_t, std::vector<uint32_t>> map;

    inline constexpr auto hash(auto param) const noexcept {
        return hashes::murmur3(param);
    }

public:
    inline void clear() {
        map.clear();
    }

    inline void insert(std::string_view prefix, uint32_t relative_pos) {
        map[hash(prefix)].push_back(relative_pos);
    }

    inline const std::vector<uint32_t> *find(
        std::string_view prefix) const {
        auto it = map.find(hash(prefix));
        if (it != map.end()) {
            return &it->second;
        } else {
            return nullptr;
        }
    }
};

