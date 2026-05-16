#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "hashes.h"

class prefix_table {
    std::unordered_map<uint32_t, std::vector<uint32_t>> map;

    constexpr auto hash(auto arg) const noexcept {
        return hashes::murmur3(arg);
    }

public:
    inline void clear() {
        map.clear();
    }

    inline void insert(std::span<std::byte> prefix, uint32_t value) {
        map[hash(prefix)].push_back(value);
    }

    inline const std::vector<uint32_t> *find(std::span<std::byte> prefix) const {
        auto it = map.find(hash(prefix));
        if (it != map.end()) {
            return &it->second;
        } else {
            return nullptr;
        }
    }
};

