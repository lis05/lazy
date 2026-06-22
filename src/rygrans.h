#pragma once

#include <smmintrin.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace rygrans {

extern "C" {
#include <ryg_rans/platform.h>
#include <ryg_rans/rans_word_sse41.h>
}

constexpr static std::byte YES_COPY{0};
constexpr static std::byte NO_COPY{1};

namespace detail {

struct Stats {
    uint16_t freq[256];
    uint32_t cum[257];
};

inline void build_stats(const uint8_t* data, size_t bytes, Stats& st) {
    uint32_t raw[256] = {};

    for (size_t i = 0; i < bytes; ++i) raw[data[i]]++;

    st.cum[0] = 0;

    for (int i = 0; i < 256; ++i) st.cum[i + 1] = st.cum[i] + raw[i];

    uint32_t total = st.cum[256];

    for (int i = 1; i <= 256; ++i)
        st.cum[i] = (uint64_t)RANS_WORD_M * st.cum[i] / total;

    for (int i = 0; i < 256; ++i) {
        if (raw[i] && st.cum[i] == st.cum[i + 1]) {
            uint32_t best = ~0u;
            int      steal = -1;

            for (int j = 0; j < 256; ++j) {
                uint32_t f = st.cum[j + 1] - st.cum[j];

                if (f > 1 && f < best) {
                    best = f;
                    steal = j;
                }
            }

            if (steal < i) {
                for (int j = steal + 1; j <= i; ++j) st.cum[j]--;
            } else {
                for (int j = i + 1; j <= steal; ++j) st.cum[j]++;
            }
        }
    }

    for (int i = 0; i < 256; ++i) st.freq[i] = st.cum[i + 1] - st.cum[i];

    if (st.cum[256] != RANS_WORD_M)
        throw std::runtime_error("bad normalization");
}

inline void build_decode_table(const uint16_t freq[256], RansWordTables& tab) {
    uint32_t start = 0;

    for (int i = 0; i < 256; ++i) {
        if (freq[i]) {
            RansWordTablesInitSymbol(&tab, (uint8_t)i, start, freq[i]);

            start += freq[i];
        }
    }

    if (start != RANS_WORD_M)
        throw std::runtime_error("invalid table");
}

}  // namespace detail

template <typename B>
inline void compress(const std::vector<B>& src, std::vector<std::byte>& dest) {
    if (src.empty()) {
        dest.clear();
        return;
    }

    size_t bytes = src.size() * sizeof(B);

    const uint8_t* input = reinterpret_cast<const uint8_t*>(src.data());

    if (bytes < 128)
        goto copy;

    {
        detail::Stats stats;

        detail::build_stats(input, bytes, stats);

        size_t out_cap = bytes + (bytes >> 3) + 128;

        std::vector<uint8_t> buf(out_cap + 16);

        uint16_t* ptr = reinterpret_cast<uint16_t*>(buf.data() + out_cap);

        RansWordEnc rans[8];

        for (int i = 0; i < 8; ++i) rans[i] = RansWordEncInit();

        for (size_t i = bytes; i > 0; --i) {
            uint8_t s = input[i - 1];

            RansWordEncPut(&rans[(i - 1) & 7], &ptr, stats.cum[s], stats.freq[s]);
        }

        for (int i = 8; i > 0; --i) RansWordEncFlush(&rans[i - 1], &ptr);

        size_t encoded = buf.data() + out_cap - reinterpret_cast<uint8_t*>(ptr);

        constexpr size_t hdr = 1 + sizeof(uint64_t) + 512;

        if (encoded + hdr >= bytes)
            goto copy;

        dest.resize(hdr + encoded);

        dest[0] = NO_COPY;

        uint64_t orig = bytes;

        std::memcpy(dest.data() + 1, &orig, sizeof(orig));

        std::memcpy(dest.data() + 1 + sizeof(uint64_t), stats.freq, 512);

        std::memcpy(dest.data() + hdr, ptr, encoded);

        return;
    }

copy:
    dest.resize(bytes + 1);

    dest[0] = YES_COPY;

    std::memcpy(dest.data() + 1, input, bytes);
}

template <typename B>
inline void decompress(const std::vector<std::byte>& src, std::vector<B>& dest) {
    if (src.empty()) {
        dest.clear();
        return;
    }

    if (src[0] == YES_COPY) {
        dest.resize((src.size() - 1) / sizeof(B));

        std::memcpy(dest.data(), src.data() + 1, src.size() - 1);

        return;
    }

    constexpr size_t hdr = 1 + sizeof(uint64_t) + 512;

    uint64_t bytes;

    std::memcpy(&bytes, src.data() + 1, sizeof(bytes));

    dest.resize(bytes / sizeof(B));

    uint16_t freq[256];

    std::memcpy(freq, src.data() + 1 + sizeof(uint64_t), 512);

    RansWordTables tab;

    detail::build_decode_table(freq, tab);

    uint16_t* ptr =
        const_cast<uint16_t*>(reinterpret_cast<const uint16_t*>(src.data() + hdr));

    uint8_t* out = reinterpret_cast<uint8_t*>(dest.data());

    RansSimdDec r0;
    RansSimdDec r1;

    RansSimdDecInit(&r0, &ptr);

    RansSimdDecInit(&r1, &ptr);

    size_t simd = bytes & ~size_t(7);

    for (size_t i = 0; i < simd; i += 8) {
        uint32_t s03 = RansSimdDecSym(&r0, &tab);

        uint32_t s47 = RansSimdDecSym(&r1, &tab);

        std::memcpy(out + i, &s03, 4);

        std::memcpy(out + i + 4, &s47, 4);

        RansSimdDecRenorm(&r0, &ptr);

        RansSimdDecRenorm(&r1, &ptr);
    }

    for (size_t i = simd; i < bytes; ++i) {
        RansSimdDec* which = (i & 4) ? &r1 : &r0;

        out[i] = RansWordDecSym(&which->lane[i & 3], &tab);
    }
}

}  // namespace rygrans
