#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>

class config {
public:
    inline static size_t block_size = 1 << 20;
    inline static size_t window_size = 1 << 16;
    inline static size_t future_limit = 1 << 12;
    inline static size_t prefix_size = 3;
    inline static size_t total_hashes() {
        return 1 << (8 * prefix_size);
    }
    inline static size_t max_matches = 0;
    inline static size_t jobs = 1;
    inline static size_t blocks = 1;
    inline static bool   print_progress = false;
    inline static uint32_t lazy_matching = 1;

    inline static int level = 0;

    inline static std::atomic_uint64_t processed_bytes;
    inline static uint64_t             total_bytes;

    static void report_progress() {
        if (!print_progress) {
            return;
        }

        do {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1000ms);
            std::cout << '\r'
                      << "Progress: " << (100 * processed_bytes / total_bytes)
                      << "% (" << processed_bytes << " / " << total_bytes << ")"
                      << std::flush;
        } while (processed_bytes != total_bytes);
        std::cout << std::endl;
    }

    static void apply_level(int l, size_t file_size) {
        auto cores = static_cast<size_t>(std::thread::hardware_concurrency());

        if (l == 0) {
            return;
        } else {
            return;
        }
    }
};
