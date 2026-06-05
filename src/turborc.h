#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <vector>

namespace turborc {
extern "C" {
// clang-format off
#include <turborc/include/turborc.h>
#include <turborc/include/anscdf.h>
// clang-format on
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

    dest.resize(bytes);
    size_t count =
        enc(const_cast<unsigned char *>(
                reinterpret_cast<const unsigned char *>(src.data())),
            bytes, reinterpret_cast<unsigned char *>(dest.data()), Params...);
    if (count == bytes) {
        dest.insert(dest.begin(), YES_COPY);
    } else {
        dest.insert(dest.begin(), NO_COPY);
    }
    dest.resize(count + 1);
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
        size_t count =
            dec(const_cast<unsigned char *>(
                    reinterpret_cast<const unsigned char *>(src.data() + 1)),
                dest.size() * sizeof(B),
                reinterpret_cast<unsigned char *>(dest.data()), Params...);
        dest.resize(count / sizeof(B));
    }
}
}  // namespace turborc
