#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace hashes {

namespace _murmur3 {

static inline constexpr uint64_t rotl64(uint64_t x, int r) noexcept {
    return (x << r) | (x >> (64 - r));
}

static inline constexpr uint64_t murmur3(std::string_view key) noexcept {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(key.data());

    const size_t len = key.size();
    const size_t nblocks = len / 8;

    uint64_t h1 = 0xdeadbeef;

    constexpr uint64_t c1 = 0x87c37b91114253d5ULL;
    constexpr uint64_t c2 = 0x4cf5ad432745937fULL;

    // --- body ---
    for (size_t i = 0; i < nblocks; ++i) {
        uint64_t k1;

        if (std::is_constant_evaluated()) {
            const size_t j = i * 8;

            k1 = uint64_t(data[j]) | (uint64_t(data[j + 1]) << 8) |
                 (uint64_t(data[j + 2]) << 16) | (uint64_t(data[j + 3]) << 24) |
                 (uint64_t(data[j + 4]) << 32) | (uint64_t(data[j + 5]) << 40) |
                 (uint64_t(data[j + 6]) << 48) | (uint64_t(data[j + 7]) << 56);
        } else {
            std::memcpy(&k1, data + i * 8, sizeof(uint64_t));
        }

        k1 *= c1;
        k1 = rotl64(k1, 31);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotl64(h1, 27);
        h1 = h1 * 5 + 0x52dce729;
    }

    // --- tail ---
    const uint8_t *tail = data + nblocks * 8;
    uint64_t       k1 = 0;

    switch (len & 7) {
    case 7:
        k1 ^= uint64_t(tail[6]) << 48;
        [[fallthrough]];
    case 6:
        k1 ^= uint64_t(tail[5]) << 40;
        [[fallthrough]];
    case 5:
        k1 ^= uint64_t(tail[4]) << 32;
        [[fallthrough]];
    case 4:
        k1 ^= uint64_t(tail[3]) << 24;
        [[fallthrough]];
    case 3:
        k1 ^= uint64_t(tail[2]) << 16;
        [[fallthrough]];
    case 2:
        k1 ^= uint64_t(tail[1]) << 8;
        [[fallthrough]];
    case 1:
        k1 ^= uint64_t(tail[0]);
        k1 *= c1;
        k1 = rotl64(k1, 31);
        k1 *= c2;
        h1 ^= k1;
    }

    // --- finalization ---
    h1 ^= len;

    h1 ^= h1 >> 33;
    h1 *= 0xff51afd7ed558ccdULL;
    h1 ^= h1 >> 33;
    h1 *= 0xc4ceb9fe1a85ec53ULL;
    h1 ^= h1 >> 33;

    return h1;
}

}  // namespace _murmur3

inline constexpr auto murmur3 = _murmur3::murmur3;

}  // namespace hashes
