#include "circular_buffer.h"

#include <format>
#include <stdexcept>

circular_buffer::circular_buffer(size_t buffer_size) {
    data.reserve(buffer_size);
    base = 0;
}

size_t circular_buffer::size() const noexcept {
    return data.size();
}

void circular_buffer::shift_by(size_t count) noexcept {
    base += count;
}

std::byte &circular_buffer::operator[](size_t index) {
    if (index >= data.size()) [[unlikely]] {
        throw std::runtime_error(
            std::format("Index {} out of bounds {}", index, data.size()));
    }
    return data[(base + index) % data.size()];
}
