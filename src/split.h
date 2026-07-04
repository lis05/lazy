#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

static inline std::vector<std::pair<uint32_t, uint32_t>> split_range(
    uint32_t size, uint32_t count, uint32_t min_block_size,
    uint32_t max_block_size) {
    uint32_t block_size = (size + count - 1) / count;
    if (block_size < min_block_size) {
        block_size = min_block_size;
    }
    if (block_size > max_block_size) {
        block_size = max_block_size;
    }

    std::vector<std::pair<uint32_t, uint32_t>> res;
    res.reserve((size + block_size - 1) / block_size);

    uint32_t start = 0;
    uint32_t end = start + block_size - 1;
    if (end >= size) {
        end = size - 1;
    }
    while (start < size) {
        res.push_back(std::pair{start, end});

        start = end + 1;
        end = start + block_size - 1;
        if (end >= size) {
            end = size - 1;
        }
    }

    return res;
}
