#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <utility>
#include <vector>

#include "config.h"
#include "token.h"

class decoder {
    std::vector<std::byte> data;

public:
    decoder() = default;

    inline void reset() {
        data.clear();
    }

    inline std::pair<const std::byte *, size_t> get_bytes() const noexcept {
        return {data.data(), data.size()};
    }

    inline void decode(const std::vector<token> &tokens) {
        for (auto t : tokens) {
            if (std::holds_alternative<std::byte>(t)) {
                data.push_back(std::get<std::byte>(t));
            } else {
                auto [distance, length] = std::get<match>(t);
                if (distance > data.size()) {
                    throw std::runtime_error(
                        std::format("Invalid distance: {}", distance));
                }
                for (size_t i = data.size() - distance, len = 0; len < length;
                     len++, i++) {
                    data.push_back(data[i]);
                }
            }
        }
    }
};
