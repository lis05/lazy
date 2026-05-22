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
    inline static size_t window_size = 1 << 15;
    inline static size_t future_limit = 18;
    inline static size_t prefix_size = 3;
    inline static size_t total_hashes() {
        return 1 << (8 * prefix_size);
    }
    inline static size_t max_matches = 1 << 29;
    inline static size_t jobs = 1;
    inline static size_t blocks = 1;
    inline static bool   print_progress = false;
    inline static bool   lazy_matching = false;

    inline static std::atomic_uint64_t processed_bytes;
    inline static uint64_t             total_bytes;

    static void report_progress() {
        if (!print_progress) {
            return;
        }

        static int next_percent = 1;

        do {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(500ms);
            while (100 * processed_bytes / total_bytes >= next_percent) {
                std::cout << "Progress: " << next_percent << "%" << std::endl;
                next_percent++;
            }
        } while (processed_bytes != total_bytes);
    }
};
