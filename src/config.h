#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>

class config {
public:
    static size_t                block_size;
    static size_t                window_size;
    static size_t                future_limit;
    static uint64_t              max_matches;
    static size_t                jobs;
    static size_t                blocks;
    static bool                  print_progress;
    static size_t                divisions;
    static std::vector<uint32_t> prefix_lengths;
    static size_t                hash_bits;
    static std::string           load_hashchains;
    static std::string           load_tokens;
    static uint32_t              passes;
    static std::string           format;
    static bool                  use_turborc;
    static bool                  use_turboans;
    static bool                  use_fse;
    static bool                  use_huf;
    static bool                  use_memcpy;
    static bool                  use_rans_static0;
    static bool                  use_rans_static1;

    static bool stats;

    static bool level[15];

    static bool                 finished;
    static std::atomic_uint64_t processed_bytes;
    static uint64_t             total_bytes;
    static void                 print_message(const std::string &message);

    static void print();
    static void finish();
    static void report_progress();
    static void apply_level(size_t file_size);
};
