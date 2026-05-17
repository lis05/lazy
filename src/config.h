#pragma once

#include <cassert>
#include <cstddef>
#include <iostream>

class config {
public:
    inline static size_t block_size = 1 << 20;
    inline static size_t window_size = 1 << 15;
    inline static size_t future_limit = 18;
    inline static size_t prefix_size = 3;
    inline static size_t total_hashes = 1 << (8 * prefix_size);

    inline static size_t binaryfmt_len3_distance_bits = 15;
    inline static size_t binaryfmt_len4_distance_bits = 15;
    inline static size_t binaryfmt_len5_distance_bits = 15;
    inline static size_t binaryfmt_len6_distance_bits = 15;
    inline static size_t binaryfmt_len7_distance_bits = 15;
    inline static size_t binaryfmt_lenx_distance_bits = 15;
    inline static size_t binaryfmt_lenx_length_bits = 4;

    static void load(size_t b_size, size_t w_size, size_t f_limit, size_t p_size,
                     size_t l3_dist, size_t l4_dist, size_t l5_dist, size_t l6_dist,
                     size_t l7_dist, size_t lx_dist, size_t lx_len) {
        block_size = b_size;
        window_size = w_size;
        future_limit = f_limit;
        prefix_size = p_size;
        total_hashes = 1 << (8 * prefix_size);
        binaryfmt_len3_distance_bits = l3_dist;
        binaryfmt_len4_distance_bits = l4_dist;
        binaryfmt_len5_distance_bits = l5_dist;
        binaryfmt_len6_distance_bits = l6_dist;
        binaryfmt_len7_distance_bits = l7_dist;
        binaryfmt_lenx_distance_bits = lx_dist;
        binaryfmt_lenx_length_bits = lx_len;
    }
};
