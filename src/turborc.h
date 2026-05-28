#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <vector>

extern "C" {
int verbose;
}

namespace turborc {
extern "C" {
// clang-format off
#include <turborc/include/turborc.h>
#include <turborc/include/anscdf.h>
// clang-format on
}

constexpr static std::byte YES_COPY{0};
constexpr static std::byte NO_COPY{1};

template <typename B>
    requires(sizeof(B) == 1)
static inline void compress(const std::vector<B>   &src,
                            std::vector<std::byte> &dest) {
    if (src.empty()) {
        dest.clear();
        return;
    }

    dest.resize(src.size());
    size_t count =
        rcmrrsenc(const_cast<unsigned char *>(
                      reinterpret_cast<const unsigned char *>(src.data())),
                  src.size(), reinterpret_cast<unsigned char *>(dest.data()));
    if (count == src.size()) {
        dest.insert(dest.begin(), YES_COPY);
    } else {
        dest.insert(dest.begin(), NO_COPY);
    }
    dest.resize(count + 1);
}

template <typename B>
    requires(sizeof(B) == 1)
static inline void decompress(const std::vector<std::byte> &src,
                              std::vector<B>               &dest) {
    if (src.empty()) {
        dest.clear();
        return;
    }

    std::byte is_copy = src[0];
    if (is_copy == YES_COPY) {
        std::copy(src.begin() + 1, src.end(), dest.begin());
        dest.resize(src.size() - 1);
    } else {
        size_t count =
            rcmrrsdec(const_cast<unsigned char *>(
                          reinterpret_cast<const unsigned char *>(src.data() + 1)),
                      dest.size(), reinterpret_cast<unsigned char *>(dest.data()));
        dest.resize(count);
    }
}
}  // namespace turborc
