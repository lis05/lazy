#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <vector>

namespace huf {
extern "C" {
#include <fse/lib/huf.h>
}

constexpr static std::byte YES_COPY{0};
constexpr static std::byte NO_COPY{1};
constexpr static size_t    MAX_BLOCK_SIZE{128 * 1024};
constexpr static uint32_t  RAW_BLOCK_FLAG{0x80000000};

template <typename B>
static inline void compress(const std::vector<B>   &src,
                            std::vector<std::byte> &dest) {
    if (src.empty()) {
        dest.clear();
        return;
    }

    size_t         bytes = src.size() * sizeof(B);
    const uint8_t *src_ptr = reinterpret_cast<const uint8_t *>(src.data());

    dest.clear();
    dest.reserve(bytes + sizeof(std::byte) + sizeof(uint64_t) +
                 (bytes / MAX_BLOCK_SIZE + 1) * sizeof(uint32_t));

    dest.push_back(NO_COPY);

    uint64_t original_bytes = static_cast<uint64_t>(bytes);
    size_t   current_size = dest.size();
    dest.resize(current_size + sizeof(uint64_t));
    std::memcpy(dest.data() + current_size, &original_bytes, sizeof(uint64_t));

    size_t offset = 0;
    bool   all_raw = true;

    while (offset < bytes) {
        size_t chunk_size = std::min(MAX_BLOCK_SIZE, bytes - offset);
        size_t bound = HUF_compressBound(chunk_size);

        size_t header_pos = dest.size();
        dest.resize(header_pos + sizeof(uint32_t) + bound);

        size_t count = HUF_compress(dest.data() + header_pos + sizeof(uint32_t),
                                    bound, src_ptr + offset, chunk_size);

        if (HUF_isError(count)) {
            throw std::runtime_error(
                std::format("Compression failed: {}", HUF_getErrorName(count)));
        }

        uint32_t block_header = 0;
        if (count == 0 || count >= chunk_size) {
            block_header = static_cast<uint32_t>(chunk_size) | RAW_BLOCK_FLAG;
            std::memcpy(dest.data() + header_pos + sizeof(uint32_t),
                        src_ptr + offset, chunk_size);
            dest.resize(header_pos + sizeof(uint32_t) + chunk_size);
        } else {
            block_header = static_cast<uint32_t>(count);
            dest.resize(header_pos + sizeof(uint32_t) + count);
            all_raw = false;
        }

        std::memcpy(dest.data() + header_pos, &block_header, sizeof(uint32_t));
        offset += chunk_size;
    }

    if (all_raw) {
        dest.resize(bytes + 1);
        std::memcpy(dest.data() + 1, src_ptr, bytes);
        dest[0] = YES_COPY;
    }
}

static inline void decompress(const std::byte *from, uint32_t bytes, std::byte *to) {
    if (bytes == 0) {
        return;
    }

    std::byte is_copy = from[0];
    if (is_copy == YES_COPY) {
        std::memcpy(to, from + 1, bytes - 1);
        return;
    }

    size_t header_size = 1 + sizeof(uint64_t);
    if (bytes < header_size) {
        throw std::runtime_error("Decompression failed: source too small");
    }

    uint64_t original_bytes;
    std::memcpy(&original_bytes, from + 1, sizeof(uint64_t));

    uint8_t *dest_ptr = reinterpret_cast<uint8_t *>(to);

    size_t src_offset = header_size;
    size_t dest_offset = 0;

    while (dest_offset < original_bytes) {
        if (src_offset + sizeof(uint32_t) > bytes) {
            throw std::runtime_error("Decompression failed: truncated block header");
        }

        uint32_t block_header;
        std::memcpy(&block_header, from + src_offset, sizeof(uint32_t));
        src_offset += sizeof(uint32_t);

        bool     is_raw = (block_header & RAW_BLOCK_FLAG) != 0;
        uint32_t payload_size = block_header & ~RAW_BLOCK_FLAG;

        if (src_offset + payload_size > bytes) {
            throw std::runtime_error(
                "Decompression failed: truncated block payload");
        }

        size_t chunk_expected_size = std::min(
            MAX_BLOCK_SIZE, static_cast<size_t>(original_bytes - dest_offset));

        if (is_raw) {
            if (payload_size != chunk_expected_size) {
                throw std::runtime_error(
                    "Decompression failed: raw block size mismatch");
            }
            std::memcpy(dest_ptr + dest_offset, from + src_offset, payload_size);
        } else {
            size_t count =
                HUF_decompress(dest_ptr + dest_offset, chunk_expected_size,
                               from + src_offset, payload_size);

            if (HUF_isError(count)) {
                throw std::runtime_error(std::format("Decompression failed: {}",
                                                     HUF_getErrorName(count)));
            }
            if (count != chunk_expected_size) {
                throw std::runtime_error(
                    "Decompression failed: decoded size mismatch");
            }
        }

        src_offset += payload_size;
        dest_offset += chunk_expected_size;
    }
}
}  // namespace huf
