#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

namespace hashes {

template <std::input_iterator It, std::sentinel_for<It> Sent>
    requires std::same_as<std::iter_value_t<It>, std::byte>
static constexpr uint64_t murmur3(It first, Sent last, uint64_t seed = 0) noexcept {
    auto fmix64 = [](uint64_t k) noexcept {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        return k;
    };

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    constexpr uint64_t c1 = 0x87c37b91114253d5ULL;
    constexpr uint64_t c2 = 0x4cf5b3d459451decULL;

    size_t len = 0;

    if constexpr (std::contiguous_iterator<It>) {
        const std::byte* ptr = std::to_address(first);
        const std::byte* end_ptr = std::to_address(last);
        len = static_cast<size_t>(end_ptr - ptr);
        size_t nblocks = len / 16;

        for (size_t i = 0; i < nblocks; ++i) {
            uint64_t k1;
            uint64_t k2;
            std::memcpy(&k1, ptr + i * 16, 8);
            std::memcpy(&k2, ptr + i * 16 + 8, 8);

            k1 *= c1;
            k1 = std::rotl(k1, 31);
            k1 *= c2;
            h1 ^= k1;
            h1 = std::rotl(h1, 27);
            h1 += h2;
            h1 = h1 * 5 + 0x52dce729;

            k2 *= c2;
            k2 = std::rotl(k2, 33);
            k2 *= c1;
            h2 ^= k2;
            h2 = std::rotl(h2, 31);
            h2 += h1;
            h2 = h2 * 5 + 0x38495ab5;
        }

        const std::byte* tail = ptr + nblocks * 16;
        size_t           tail_len = len % 16;
        uint64_t         k1 = 0;
        uint64_t         k2 = 0;

        switch (tail_len) {
        case 15:
            k2 ^= static_cast<uint64_t>(tail[14]) << 48;
            [[fallthrough]];
        case 14:
            k2 ^= static_cast<uint64_t>(tail[13]) << 40;
            [[fallthrough]];
        case 13:
            k2 ^= static_cast<uint64_t>(tail[12]) << 32;
            [[fallthrough]];
        case 12:
            k2 ^= static_cast<uint64_t>(tail[11]) << 24;
            [[fallthrough]];
        case 11:
            k2 ^= static_cast<uint64_t>(tail[10]) << 16;
            [[fallthrough]];
        case 10:
            k2 ^= static_cast<uint64_t>(tail[9]) << 8;
            [[fallthrough]];
        case 9:
            k2 ^= static_cast<uint64_t>(tail[8]);
            k2 *= c2;
            k2 = std::rotl(k2, 33);
            k2 *= c1;
            h2 ^= k2;
            [[fallthrough]];
        case 8:
            k1 ^= static_cast<uint64_t>(tail[7]) << 56;
            [[fallthrough]];
        case 7:
            k1 ^= static_cast<uint64_t>(tail[6]) << 48;
            [[fallthrough]];
        case 6:
            k1 ^= static_cast<uint64_t>(tail[5]) << 40;
            [[fallthrough]];
        case 5:
            k1 ^= static_cast<uint64_t>(tail[4]) << 32;
            [[fallthrough]];
        case 4:
            k1 ^= static_cast<uint64_t>(tail[3]) << 24;
            [[fallthrough]];
        case 3:
            k1 ^= static_cast<uint64_t>(tail[2]) << 16;
            [[fallthrough]];
        case 2:
            k1 ^= static_cast<uint64_t>(tail[1]) << 8;
            [[fallthrough]];
        case 1:
            k1 ^= static_cast<uint64_t>(tail[0]);
            k1 *= c1;
            k1 = std::rotl(k1, 31);
            k1 *= c2;
            h1 ^= k1;
        };

    } else {
        std::byte block[16];
        size_t    block_len = 0;

        while (first != last) {
            block[block_len++] = *first;
            ++first;

            if (block_len == 16) {
                uint64_t k1;
                uint64_t k2;
                std::memcpy(&k1, block, 8);
                std::memcpy(&k2, block + 8, 8);

                k1 *= c1;
                k1 = std::rotl(k1, 31);
                k1 *= c2;
                h1 ^= k1;
                h1 = std::rotl(h1, 27);
                h1 += h2;
                h1 = h1 * 5 + 0x52dce729;

                k2 *= c2;
                k2 = std::rotl(k2, 33);
                k2 *= c1;
                h2 ^= k2;
                h2 = std::rotl(h2, 31);
                h2 += h1;
                h2 = h2 * 5 + 0x38495ab5;

                len += 16;
                block_len = 0;
            }
        }
        len += block_len;

        if (block_len > 0) {
            uint64_t k1 = 0;
            uint64_t k2 = 0;
            switch (block_len) {
            case 15:
                k2 ^= static_cast<uint64_t>(block[14]) << 48;
                [[fallthrough]];
            case 14:
                k2 ^= static_cast<uint64_t>(block[13]) << 40;
                [[fallthrough]];
            case 13:
                k2 ^= static_cast<uint64_t>(block[12]) << 32;
                [[fallthrough]];
            case 12:
                k2 ^= static_cast<uint64_t>(block[11]) << 24;
                [[fallthrough]];
            case 11:
                k2 ^= static_cast<uint64_t>(block[10]) << 16;
                [[fallthrough]];
            case 10:
                k2 ^= static_cast<uint64_t>(block[9]) << 8;
                [[fallthrough]];
            case 9:
                k2 ^= static_cast<uint64_t>(block[8]);
                k2 *= c2;
                k2 = std::rotl(k2, 33);
                k2 *= c1;
                h2 ^= k2;
                [[fallthrough]];
            case 8:
                k1 ^= static_cast<uint64_t>(block[7]) << 56;
                [[fallthrough]];
            case 7:
                k1 ^= static_cast<uint64_t>(block[6]) << 48;
                [[fallthrough]];
            case 6:
                k1 ^= static_cast<uint64_t>(block[5]) << 40;
                [[fallthrough]];
            case 5:
                k1 ^= static_cast<uint64_t>(block[4]) << 32;
                [[fallthrough]];
            case 4:
                k1 ^= static_cast<uint64_t>(block[3]) << 24;
                [[fallthrough]];
            case 3:
                k1 ^= static_cast<uint64_t>(block[2]) << 16;
                [[fallthrough]];
            case 2:
                k1 ^= static_cast<uint64_t>(block[1]) << 8;
                [[fallthrough]];
            case 1:
                k1 ^= static_cast<uint64_t>(block[0]);
                k1 *= c1;
                k1 = std::rotl(k1, 31);
                k1 *= c2;
                h1 ^= k1;
            };
        }
    }

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
