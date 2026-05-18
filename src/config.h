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

    inline static size_t max_matches = 1 << 29;

    static void load(size_t b_size, size_t w_size, size_t f_limit, size_t max_m) {
        block_size = b_size;
        window_size = w_size;
        future_limit = f_limit;
        total_hashes = 1 << (8 * prefix_size);
        max_matches = max_m;
    }
};
