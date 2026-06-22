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
    } else {
        dest.resize(count + header_size);
        uint64_t original_bytes = static_cast<uint64_t>(bytes);
        std::memcpy(dest.data() + 1, &original_bytes, sizeof(uint64_t));
        dest[0] = NO_COPY;
    }
}

template <typename B>
static inline void decompress(const std::vector<std::byte> &src,
                              std::vector<B>               &dest) {
    if (src.empty()) {
        dest.clear();
        return;
    }

    std::byte is_copy = src[0];
    if (is_copy == YES_COPY) {
        dest.resize((src.size() - 1) / sizeof(B));
        std::memcpy(dest.data(), src.data() + 1, src.size() - 1);
    } else {
        size_t header_size = 1 + sizeof(uint64_t);
        if (src.size() < header_size) {
            throw std::runtime_error("Decompression failed: source too small");
        }

        uint64_t original_bytes;
        std::memcpy(&original_bytes, src.data() + 1, sizeof(uint64_t));

        dest.resize(original_bytes / sizeof(B));

        size_t count =
            FSE_decompress(dest.data(), original_bytes, src.data() + header_size,
                           src.size() - header_size);

        if (FSE_isError(count)) {
            throw std::runtime_error(
                std::format("Decompression failed: {}", FSE_getErrorName(count)));
        }
    }
}
}  // namespace fse
