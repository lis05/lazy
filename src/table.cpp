#include "table.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

table::table(size_t bits) {
    mask = (uint32_t{1} << bits) - 1;
    data.resize(mask + 1);
    present.resize(mask + 1, false);
}
void table::insert(uint32_t hash, uint32_t value) {
    data[hash & mask] = value;
    present[hash & mask] = true;
}
bool table::has(uint32_t hash) {
    return present[hash & mask];
}
uint32_t table::get(uint32_t hash) {
    return data[hash & mask];
}
void table::clear() {
    std::fill(present.begin(), present.end(), false);
}
void table::destroy() {
    std::vector<bool>().swap(present);
    std::vector<uint32_t>().swap(data);
}
