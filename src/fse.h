#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <vector>
namespace fse {
extern "C" {
#include <fse/lib/fse.h>
}
constexpr static std::byte YES_COPY{0};
constexpr static std::byte NO_COPY{1};
constexpr static std::byte RLE{2};

template <typename B>
static inline void compress(const std::vector<B>   &src,
                            std::vector<std::byte> &dest) {
    if (src.empty()) {
        dest.clear();
        return;
    }
    size_t bytes = src.size() * sizeof(B);
    size_t header_size = 1 + sizeof(uint64_t);
    dest.resize(FSE_compressBound(bytes) + header_size);
    size_t count = FSE_compress(dest.data() + header_size, dest.size() - header_size,
                                src.data(), bytes);
    if (FSE_isError(count)) {
        throw std::runtime_error(
            std::format("Compression failed: {}", FSE_getErrorName(count)));
    }
    if (count == 0) {
        dest.resize(bytes + 1);
        std::memcpy(dest.data() + 1, src.data(), bytes);
        dest[0] = YES_COPY;
    } else if (count == 1) {
        dest.resize(1 + sizeof(uint64_t) + 1);
        dest[0] = RLE;
        uint64_t original_bytes = static_cast<uint64_t>(bytes);
        std::memcpy(dest.data() + 1, &original_bytes, sizeof(uint64_t));
        dest[1 + sizeof(uint64_t)] = dest[header_size];
    } else {
        dest.resize(count + header_size);
        uint64_t original_bytes = static_cast<uint64_t>(bytes);
        std::memcpy(dest.data() + 1, &original_bytes, sizeof(uint64_t));
        dest[0] = NO_COPY;
    }
}

static inline void decompress(const std::byte *from, uint32_t bytes, std::byte *to) {
    if (bytes == 0) {
        return;
    }
    std::byte flag = from[0];
    if (flag == YES_COPY) {
        std::memcpy(to, from + 1, bytes - 1);
    } else if (flag == RLE) {
        size_t header_size = 1 + sizeof(uint64_t);
        if (bytes < header_size + 1) {
            throw std::runtime_error("Decompression failed: RLE source too small");
        }
        uint64_t original_bytes;
        std::memcpy(&original_bytes, from + 1, sizeof(uint64_t));
        std::byte symbol = from[header_size];
        std::memset(to, static_cast<int>(symbol), original_bytes);
    } else {
        size_t header_size = 1 + sizeof(uint64_t);
        if (bytes < header_size) {
            throw std::runtime_error("Decompression failed: source too small");
        }
        uint64_t original_bytes;
        std::memcpy(&original_bytes, from + 1, sizeof(uint64_t));
        size_t count = FSE_decompress(to, original_bytes, from + header_size,
                                      bytes - header_size);
        if (FSE_isError(count)) {
            throw std::runtime_error(
                std::format("Decompression failed: {}", FSE_getErrorName(count)));
        }
    }
}
}  // namespace fse
