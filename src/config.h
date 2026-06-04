#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>

class config {
public:
    static size_t block_size;
    static size_t window_size;
    static size_t future_limit;
    static size_t prefix_size;
    static size_t total_hashes() {
        return 1 << (8 * prefix_size);
    }
    static uint64_t    max_matches;
    static size_t      jobs;
    static size_t      blocks;
    static bool        print_progress;
    static uint32_t    lazy_matching;
    static size_t      divisions;
    static size_t      max_prefix_lengths;
    static std::string format;

    static int level;

    static std::atomic_uint64_t processed_bytes;
    static uint64_t             total_bytes;

    static void print();
    static void report_progress();
    static void apply_level(int l, size_t file_size);
};
