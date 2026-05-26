#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace hashes {
uint32_t static inline hash3(std::byte *data) {
    return ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2]);
}
uint32_t static inline hash2(std::byte *data) {
    return ((uint32_t)data[0] << 8) | ((uint32_t)data[1]);
}
}  // namespace hashes
