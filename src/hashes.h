#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <cstring>

namespace hashes {

[[nodiscard]] static inline std::uint64_t murmur3(std::span<const std::byte> data,
                                           std::uint64_t seed = 0) noexcept {
    constexpr std::uint64_t c1 = 0x87c37b91114253d5ULL;
    constexpr std::uint64_t c2 = 0x4cf5ad432745937fULL;

    auto rotl64 = [](std::uint64_t x, int r) constexpr noexcept {
        return std::rotl(x, r);
    };

    auto fmix64 = [](std::uint64_t k) constexpr noexcept {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        return k;
    };

    const auto*       bytes = reinterpret_cast<const std::uint8_t*>(data.data());
    const std::size_t len = data.size();

    std::uint64_t h1 = seed;
    std::uint64_t h2 = seed;

    const std::size_t nblocks = len / 16;

    // body
    for (std::size_t i = 0; i < nblocks; ++i) {
        std::uint64_t k1;
        std::uint64_t k2;

        std::memcpy(&k1, bytes + i * 16, 8);
        std::memcpy(&k2, bytes + i * 16 + 8, 8);

        if constexpr (std::endian::native == std::endian::big) {
            k1 = std::byteswap(k1);
            k2 = std::byteswap(k2);
        }

        k1 *= c1;
        k1 = rotl64(k1, 31);
        k1 *= c2;
        h1 ^= k1;

        h1 = rotl64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729ULL;

        k2 *= c2;
        k2 = rotl64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

        h2 = rotl64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5ULL;
    }

    // tail
    const auto* tail = bytes + nblocks * 16;

    std::uint64_t k1 = 0;
    std::uint64_t k2 = 0;

    switch (len & 15) {
    case 15:
        k2 ^= std::uint64_t(tail[14]) << 48;
    case 14:
        k2 ^= std::uint64_t(tail[13]) << 40;
    case 13:
        k2 ^= std::uint64_t(tail[12]) << 32;
    case 12:
        k2 ^= std::uint64_t(tail[11]) << 24;
    case 11:
        k2 ^= std::uint64_t(tail[10]) << 16;
    case 10:
        k2 ^= std::uint64_t(tail[9]) << 8;
    case 9:
        k2 ^= std::uint64_t(tail[8]);
        k2 *= c2;
        k2 = rotl64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

    case 8:
        k1 ^= std::uint64_t(tail[7]) << 56;
    case 7:
        k1 ^= std::uint64_t(tail[6]) << 48;
    case 6:
        k1 ^= std::uint64_t(tail[5]) << 40;
    case 5:
        k1 ^= std::uint64_t(tail[4]) << 32;
    case 4:
        k1 ^= std::uint64_t(tail[3]) << 24;
    case 3:
        k1 ^= std::uint64_t(tail[2]) << 16;
    case 2:
        k1 ^= std::uint64_t(tail[1]) << 8;
    case 1:
        k1 ^= std::uint64_t(tail[0]);
        k1 *= c1;
        k1 = rotl64(k1, 31);
        k1 *= c2;
        h1 ^= k1;

    default:
        break;
    }

    // finalization
    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;

    return h1;
}

}  // namespace hashes
