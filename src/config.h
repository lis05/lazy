#pragma once

#include <cassert>
#include <cstddef>
#include <iostream>

class config {
public:
    static constexpr size_t block_size = 1 << 20;
    static constexpr size_t window_size = 4096;
    static constexpr size_t future_limit = 16;
    static constexpr size_t prefix_size = 3;
    static constexpr size_t total_hashes = 1 << (8 * prefix_size);
};

