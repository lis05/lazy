#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

class table {
    size_t size;
    uint64_t mask;
    std::vector<bool> present;
    std::vector<uint32_t> data;
public:
    table(size_t bits);
    void insert(uint32_t hash, uint32_t value);
    bool has(uint32_t hash);
    uint32_t get(uint32_t hash);
    void clear();
    void destroy();
};
