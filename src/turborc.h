#pragma once

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
    dest.resize(count);
}

template <typename B>
    requires(sizeof(B) == 1)
static inline void decompress(const std::vector<std::byte> &src,
                              std::vector<B>               &dest) {
    if (src.empty()) {
        dest.clear();
        return;
    }

    size_t count =
        rcmrrsdec(const_cast<unsigned char *>(
                      reinterpret_cast<const unsigned char *>(src.data())),
                  dest.size(), reinterpret_cast<unsigned char *>(dest.data()));
    dest.resize(count);
}
}  // namespace turborc
