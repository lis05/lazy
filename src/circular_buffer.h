#pragma once

#include <cstddef>
#include <vector>

class circular_buffer {
    std::vector<std::byte> data;
    size_t                 base;

public:
    circular_buffer(size_t buffer_size);

    size_t size() const noexcept;

    void shift_by(size_t count) noexcept;

    std::byte &operator[](size_t index);
};
