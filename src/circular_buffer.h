#pragma once

#include <cstddef>
#include <format>
#include <iterator>
#include <stdexcept>
#include <vector>

class circular_buffer {
    std::vector<std::byte> data;
    size_t                 base;

public:
    class _iterator {
        circular_buffer &buf;
        size_t           index;

    public:
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::byte;
        using difference_type = std::ptrdiff_t;
        using reference = std::byte &;
        using pointer = std::byte *;

        _iterator(circular_buffer &buf, size_t index) noexcept
            : buf(buf), index(index) {
        }

        inline bool operator==(const _iterator &other) const noexcept {
            return index == other.index;
        }

        _iterator &operator++() {
            if (index == buf.size()) {
                return *this;
            }
            index++;
            return *this;
        }

        _iterator operator++(int) {
            auto res = *this;
            this->operator++();
            return res;
        }

        inline reference operator*() const {
            return buf[index];
        }

        inline pointer operator->() const {
            return &buf[index];
        }
    };
    using iterator = _iterator;

    inline circular_buffer(size_t buffer_size) : data(buffer_size), base(0) {
    }

    inline size_t size() const noexcept {
        return data.size();
    }

    inline void shift_by(size_t count) noexcept {
        if (data.empty()) [[unlikely]]
            return;
        base += count;
        base %= data.size();
    }

    inline std::byte &operator[](size_t index) {
        if (index >= data.size()) [[unlikely]] {
            throw std::runtime_error(
                std::format("Index {} out of bounds {}", index, data.size()));
        }
        return data[(base + index) % data.size()];
    }

    iterator begin() {
        return iterator(*this, 0);
    }

    iterator end() {
        return iterator(*this, data.size());
    }
};
