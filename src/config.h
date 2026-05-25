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
    inline static bool   optimal = false;

    inline static int level = 1;

    inline static std::atomic_uint64_t processed_bytes;
    inline static uint64_t             total_bytes;

    static void report_progress() {
        if (!print_progress) {
            return;
        }

        static int next_percent = 1;

        do {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1000ms);
            while (100 * processed_bytes / total_bytes >= next_percent) {
                std::cout << "Progress: " << next_percent << "%" << std::endl;
                next_percent++;
            }
        } while (processed_bytes != total_bytes);
    }

    static void apply_level(int l, size_t file_size) {
        auto cores = static_cast<size_t>(std::thread::hardware_concurrency());

        if (l == -1) {
            return;
        } else if (l == 1) {
            blocks = 1;
            block_size = 1 << 20;
            window_size = 1 << 20;
            future_limit = 1 << 12;
            max_matches = 10;
            lazy_matching = false;
            optimal = false;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 2) {
            blocks = 1;
            block_size = 1 << 20;
            window_size = 1 << 16;
            future_limit = 1 << 20;
            max_matches = 0;
            lazy_matching = false;
            optimal = false;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 3) {
            blocks = 1;
            block_size = 1 << 20;
            window_size = 1 << 18;
            future_limit = 1 << 16;
            max_matches = 1000;
            lazy_matching = false;
            optimal = false;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 4) {
            blocks = 1;
            block_size = 1 << 20;
            window_size = 1 << 18;
            future_limit = 1 << 20;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 5) {
            blocks = 1;
            block_size = 1 << 20;
            window_size = 1 << 20;
            future_limit = 1 << 18;
            max_matches = 1000;
            lazy_matching = false;
            optimal = true;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 6) {
            blocks = 1;
            block_size = 1 << 20;
            window_size = 1 << 18;
            future_limit = 1 << 18;
            max_matches = 1000;
            lazy_matching = false;
            optimal = true;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 7) {
            blocks = 1;
            block_size = 1 << 20;
            window_size = 1 << 18;
            future_limit = 1 << 12;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 8) {
            blocks = 1;
            block_size = 1 << 20;
            window_size = 1 << 20;
            future_limit = 1 << 20;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 9) {
            blocks = 1;
            block_size = 1 << 21;
            window_size = 1 << 21;
            future_limit = 1 << 21;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 10) {
            blocks = 1;
            block_size = 1 << 22;
            window_size = 1 << 22;
            future_limit = 1 << 22;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 11) {
            blocks = 1;
            block_size = 1 << 23;
            window_size = 1 << 23;
            future_limit = 1 << 23;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = std::min(cores, file_size / block_size - file_size + 1);
        } else if (l == 12) {
            blocks = 1;
            block_size = file_size / 8;
            window_size = block_size;
            future_limit = block_size;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = 8;
        } else if (l == 13) {
            blocks = 1;
            block_size = file_size / 4;
            window_size = block_size;
            future_limit = block_size;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = 4;
        } else if (l == 14) {
            blocks = 1;
            block_size = file_size / 2;
            window_size = block_size;
            future_limit = block_size;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = 2;
        } else if (l == 15) {
            blocks = 1;
            block_size = file_size;
            window_size = block_size;
            future_limit = block_size;
            max_matches = 0;
            lazy_matching = false;
            optimal = true;
            jobs = 1;
        } else {
            return;
        }
    }
};
