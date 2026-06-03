#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <vector>

namespace fse {

extern "C" {
#include <finitestateentropy/lib/fse.h>
}

enum flag : uint8_t {
    FSE_NORMAL,
    FSE_RAW,
    FSE_RLE
};

template <typename B>
    requires(sizeof(B) == 1)
static inline void compress(const std::vector<B> &src, std::vector<std::byte> &dest,
                            flag &flag, uint32_t &dest_bytes) {
    if (src.empty()) {
        flag = FSE_RAW;
        dest_bytes = 0;
        return;
    }
    dest.resize(FSE_compressBound(src.size()));
    size_t res = FSE_compress(dest.data(), dest.size(), src.data(), src.size());
    if (FSE_isError(res)) {
        throw std::runtime_error(
            std::format("Compression failed: {}", FSE_getErrorName(res)));
    }
    if (res == 0) {
        flag = FSE_RAW;
        dest_bytes = src.size();
    } else if (res == 1) {
        flag = FSE_RLE;
        dest_bytes = 1;
    } else {
        flag = FSE_NORMAL;
        dest.resize(res);
        dest_bytes = res;
    }
}

static inline void write(auto &out, flag flag, uint32_t bytes, const std::byte *raw,
                         const std::byte *compressed) {
    if (bytes == 0) {
        return;
    }
    if (flag == FSE_RAW || flag == FSE_RLE) {
        out.write(reinterpret_cast<const char *>(raw), bytes);
    } else {
        out.write(reinterpret_cast<const char *>(compressed), bytes);
    }
}

template <typename B>
    requires(sizeof(B) == 1)
static inline void decompress(const std::vector<std::byte> &src,
                              std::vector<B> &dest, flag flag,
                              uint32_t original_size) {
    if (original_size == 0) {
        dest.clear();
        return;
    }

    dest.resize(original_size);

    if (flag == FSE_RAW) {
        std::memcpy(dest.data(), src.data(), original_size);
    } else if (flag == FSE_RLE) {
        std::fill(dest.begin(), dest.end(), static_cast<B>(src[0]));
    } else {
        size_t res =
            FSE_decompress(dest.data(), dest.size(), src.data(), src.size());
        if (FSE_isError(res)) {
            throw std::runtime_error(
                std::format("Decompression failed: {}", FSE_getErrorName(res)));
        }
    }
}

static inline auto read(auto &in, uint32_t bytes) {
    std::vector<std::byte> buf(bytes);
    if (bytes > 0) {
        in.read(reinterpret_cast<char *>(buf.data()), bytes);
    }
    return buf;
}
}  // namespace fse
