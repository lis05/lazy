#include "table.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>


table::table(size_t bits) {
    mask = (uint64_t{1} << bits) - 1;
    data.resize(static_cast<uint64_t>(mask) + 1, NONE32);
    data_ptr = data.data();
}
void table::insert(uint32_t hash, uint32_t value) {
    data_ptr[hash & mask] = value;
}
uint32_t table::get(uint32_t hash) {
    return data_ptr[hash & mask];
}
void table::clear() {
    std::fill(data.begin(), data.end(), NONE32);
}
