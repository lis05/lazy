#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "a5hash.h"

namespace hashes {
uint32_t static inline hash3(const std::byte *data) {
    return ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2]);
}
uint32_t static inline hash2(const std::byte *data) {
    return ((uint32_t)data[0] << 8) | ((uint32_t)data[1]);
}
uint32_t static inline hashn(const std::byte *data, size_t len) {
    return a5hash(static_cast<const void *>(data), len, 0) & 0xFFFFFFFF;
}
}  // namespace hashes
