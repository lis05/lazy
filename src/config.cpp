#include "config.h"

#include <unistd.h>

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <thread>

size_t              config::block_size = 1 << 20;
size_t              config::window_size = 1 << 16;
size_t              config::future_limit = 1 << 12;
size_t              config::prefix_size = 3;
uint64_t            config::max_matches = 0;
size_t              config::jobs = 1;
size_t              config::blocks = 1;
bool                config::print_progress = false;
uint32_t            config::lazy_matching = 1;
size_t              config::divisions = 1;
std::vector<size_t> config::prefix_lengths{};
bool                config::optimal_encoder = false;

std::string config::format = "ctx";

int config::level = 0;

std::atomic_uint64_t config::processed_bytes;
uint64_t             config::total_bytes;

void config::print() {
    std::cout << std::format(
                     "block_size: {}\n"
                     "window_size: {}\n"
                     "future_limit: {}\n"
                     "prefix_size: {}\n"
                     "max_matches: {}\n"
                     "jobs: {}\n"
                     "blocks: {}\n"
                     "lazy_matching: {}\n"
                     "divisions: {}\n"
                     "prefix_lengths: {}\n"
                     "optimal_encoder: {}\n"
                     "format: {}\n"
                     "level: {}",
                     block_size, window_size, future_limit, prefix_size, max_matches,
                     jobs, blocks, lazy_matching, divisions, prefix_lengths,
                     optimal_encoder, format, level)
              << std::endl;
}

static auto get_mb() {
    std::ifstream      stream("/proc/self/statm");
    unsigned long long vm_pages = 0;
    if (stream >> vm_pages) {
        unsigned long long page_size = sysconf(_SC_PAGESIZE);
        return (vm_pages * page_size) / (1024 * 1024);
    }
    return 0ULL;
}

void config::report_progress() {
    if (!print_progress)
        return;

    auto         start_time = std::chrono::system_clock::now();
    double       smoothed_rate = 0.0;
    const double alpha = 0.2;

    do {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1000ms);

        auto now = std::chrono::system_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time)
                .count();
        auto current_processed = processed_bytes.load();

        if (elapsed > 0 && current_processed > 0) {
            double current_rate = static_cast<double>(current_processed) / elapsed;
            smoothed_rate =
                (smoothed_rate == 0.0)
                    ? current_rate
                    : (alpha * current_rate + (1.0 - alpha) * smoothed_rate);
        }

        auto remaining_bytes = total_bytes - current_processed;
        auto eta = (smoothed_rate > 0)
                       ? static_cast<int64_t>(remaining_bytes / smoothed_rate)
                       : 0;
        auto total_estimated = elapsed + eta;

        std::cout << std::format(
                         "\rProgress: {}% ({} / {}, Elapsed: {}s, Total Est: {}s, "
                         "MEM: {}MB)",
                         100 * current_processed / total_bytes, current_processed,
                         total_bytes, elapsed, total_estimated, get_mb())
                  << std::flush;

    } while (processed_bytes.load() < total_bytes);
    std::cout << std::endl;
}

void config::apply_level(int l, size_t file_size) {
    auto cores = static_cast<size_t>(std::thread::hardware_concurrency());

    if (l == 0) {
        return;
    } else if (l == 1) {
        block_size = 1 << 20;
        window_size = 1 << 16;
        future_limit = 1 << 18;
        max_matches = 10;
        lazy_matching = 3;
        jobs = cores;
        divisions = 1;
        format = "ctx";
    } else if (l == 2) {
        block_size = 1 << 20;
        window_size = 1 << 20;
        future_limit = 1 << 16;
        max_matches = 10;
        lazy_matching = 2;
        jobs = cores;
        divisions = 1;
        format = "ctx";
    } else if (l == 3) {
        block_size = 1 << 20;
        window_size = 1 << 18;
        future_limit = 1 << 20;
        max_matches = 1000;
        lazy_matching = 0;
        jobs = cores;
        divisions = 1;
        format = "ctx";
    } else if (l == 4) {
        block_size = 1 << 20;
        window_size = 1 << 16;
        future_limit = 1 << 20;
        max_matches = 0;
        lazy_matching = 0;
        jobs = cores;
        divisions = 1;
        format = "turbo2";
    } else if (l == 5) {
        block_size = 1 << 20;
        window_size = 1 << 18;
        future_limit = 1 << 20;
        max_matches = 1000;
        lazy_matching = 0;
        jobs = cores;
        divisions = 1;
        format = "turbo2";
    } else if (l == 6) {
        block_size = 1 << 20;
        window_size = 1 << 18;
        future_limit = 1 << 18;
        max_matches = 1000;
        lazy_matching = 1;
        jobs = cores;
        divisions = 1;
        format = "turbo2";
    } else if (l == 7) {
        block_size = 1 << 20;
        window_size = 1 << 18;
        future_limit = 1 << 18;
        max_matches = 1000;
        lazy_matching = 2;
        jobs = cores;
        divisions = 1;
        format = "turbo2";
    } else if (l == 8) {
        block_size = 1 << 20;
        window_size = 1 << 20;
        future_limit = 1 << 16;
        max_matches = 1000;
        lazy_matching = 1;
        jobs = cores;
        divisions = 1;
        format = "turbo2";
    } else if (l == 9) {
        block_size = 1 << 20;
        window_size = 1 << 20;
        future_limit = 1 << 16;
        max_matches = 0;
        lazy_matching = 1;
        jobs = cores;
        divisions = 1;
        format = "turbo2";
    } else if (l == 10) {
        block_size = 1 << 20;
        window_size = 1 << 20;
        future_limit = 1 << 20;
        max_matches = 0;
        lazy_matching = 2;
        jobs = cores;
        divisions = 1;
        format = "turbo2";
    } else if (11 <= l && l <= 15) {
        block_size = 1 << (22 + l - 11);
        window_size = block_size;
        future_limit = block_size;
        max_matches = 0;
        lazy_matching = 2;
        format = "turbo2";

        jobs = 1;
        divisions = 2;
        while (jobs * divisions < cores && jobs * block_size * 2 <= file_size) {
            jobs *= 2;
        }
        while (jobs * divisions < cores) {
            divisions *= 2;
        }
        divisions = std::min(divisions, cores / 2);
    } else if (16 <= l && l <= 20) {
        size_t parts = 1 << (20 - l);
        block_size = file_size / parts;
        window_size = block_size;
        future_limit = block_size;
        max_matches = 0;
        lazy_matching = 2;
        format = "turbo2";
        jobs = (parts == 1 ? 1 : std::min(size_t{2}, cores));
        divisions = std::min(cores / 2, std::max(cores / jobs, size_t{1}));
    } else {
        throw std::runtime_error("Invalid level: " + std::to_string(l));
    }
}
