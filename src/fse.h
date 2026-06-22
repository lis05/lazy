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

template <auto Enc, typename B, size_t... Params>
static inline void compress(const std::vector<B>   &src,
                            std::vector<std::byte> &dest) {
    auto *enc = Enc;
    if (src.empty()) {
        dest.clear();
        return;
    }

    size_t bytes = src.size() * sizeof(B);
    dest.resize(FSE_compressBound(bytes));

    size_t count = enc(dest.data(), dest.size(), src.data(), bytes, Params...);

    if (FSE_isError(count)) {
        throw std::runtime_error(
            std::format("Compression failed: {}", FSE_getErrorName(count)));
    }

    if (count == 0) {
        dest.resize(bytes + 1);
        std::memcpy(dest.data() + 1, src.data(), bytes);
        dest[0] = YES_COPY;
    } else {
        dest.resize(count + 1);
        std::memmove(dest.data() + 1, dest.data(), count);
        dest[0] = NO_COPY;
    }
}

template <auto Dec, typename B, size_t... Params>
static inline void decompress(const std::vector<std::byte> &src,
                              std::vector<B>               &dest) {
    auto *dec = Dec;
    if (src.empty()) {
        dest.clear();
        return;
    }

    std::byte is_copy = src[0];
    if (is_copy == YES_COPY) {
        dest.resize((src.size() - 1) / sizeof(B));
        std::memcpy(dest.data(), src.data() + 1, src.size() - 1);
    } else {
        size_t count = dec(dest.data(), dest.size() * sizeof(B), src.data() + 1,
                           src.size() - 1, Params...);

        if (FSE_isError(count)) {
            throw std::runtime_error(
                std::format("Decompression failed: {}", FSE_getErrorName(count)));
        }
        dest.resize(count / sizeof(B));
    }
}
}  // namespace fse
