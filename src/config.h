#pragma once

#include <cassert>
#include <cstddef>
#include <iostream>

class config {
public:
    size_t window_size;
    size_t history_size;
    size_t prefix_size;

    config(size_t _window_size, size_t _history_size, size_t _prefix_size)
        : window_size(_window_size),
          history_size(_history_size),
          prefix_size(_prefix_size) {
        if (window_size == 0) {
            std::cerr << "Window size must not be zero." << std::endl;
            std::exit(-1);
        }
        if (history_size == 0) {
            std::cerr << "History buffer size must not be zero." << std::endl;
            std::exit(-1);
        }
        if (prefix_size == 0) {
            std::cerr << "Maximal size of a prefix to hash must not be zero."
                      << std::endl;
            std::exit(-1);
        }
    }
};

