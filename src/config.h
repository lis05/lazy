#pragma once

#include <cstddef>
#include <cassert>

class config {
public:
    size_t window_size;  // must be equal to the sum of history and future
    size_t history_size;
    size_t future_size;

    size_t prefix_size;  // size of the prefixes that will be hashed. Must be at
                         // least zero. Must also be at most future_size + 1.

    config(size_t _history_size, size_t _future_size, size_t _prefix_size)
        : window_size(_history_size + _future_size),
          history_size(history_size),
          future_size(_future_size),
          prefix_size(_prefix_size) {
        assert(window_size > 0);
        assert(history_size > 0);
        assert(future_size > 0);
        assert(prefix_size > 0);
        assert(prefix_size <= future_size + 1);
    }
};

