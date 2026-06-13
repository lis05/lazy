#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>

class config {
public:
    static size_t              block_size;
    static size_t              window_size;
    static size_t              future_limit;
    static uint64_t            max_matches;
    static size_t              jobs;
    static size_t              blocks;
    static bool                print_progress;
    static size_t              divisions;
    static std::vector<size_t> prefix_lengths;
    static size_t              hash_bits;
    static uint32_t            subblock_size;
    static std::string         format;

    static bool stats;

    static int level;

    static bool                 finished;
    static std::atomic_uint64_t processed_bytes;
    static uint64_t             total_bytes;
    static void                 print_message(const std::string &message);

    static void print();
    static void finish();
    static void report_progress();
    static void apply_level(int l, size_t file_size);
};
