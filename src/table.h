#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "config.h"

class table {
    size_t                size;
    uint32_t              mask;
    std::vector<uint32_t> data;
    uint32_t*             data_ptr;

public:
    table(size_t bits);
    void     insert(uint32_t hash, uint32_t value);
    uint32_t get(uint32_t hash);
    void     clear();
};
