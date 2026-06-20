#include "config.h"

#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

size_t                config::block_size = 1 << 20;
size_t                config::window_size = 1 << 16;
size_t                config::future_limit = 1 << 12;
uint64_t              config::max_matches = 0;
size_t                config::jobs = 1;
size_t                config::blocks = 1;
bool                  config::print_progress = false;
size_t                config::divisions = 1;
std::vector<uint32_t> config::prefix_lengths{};
size_t                config::hash_bits = 0;
bool                  config::finished = false;
std::atomic_uint64_t  config::processed_bytes;
uint64_t              config::total_bytes;
std::string           config::load_hashchains = "";
uint32_t              config::passes = 1;
std::string           config::format = "turbo2";
bool                  config::level[15] = {};

bool config::stats = false;

void config::print() {
    int l = 0;
    for (int i = 0; i < sizeof(config::level) / sizeof(config::level[0]); i++) {
        if (level[i])
            l = i;
    }
    std::cout << std::format(
                     "block_size: {}\n"
                     "window_size: {}\n"
                     "future_limit: {}\n"
                     "max_matches: {}\n"
                     "jobs: {}\n"
                     "blocks: {}\n"
                     "print_progress: {}\n"
                     "divisions: {}\n"
                     "prefix_lengths: {}\n"
                     "hash_bits: {}\n"
                     "finished: {}\n"
                     "processed_bytes: {}\n"
                     "total_bytes: {}\n"
                     "format: {}\n"
                     "level: {}\n"
                     "stats: {}",
                     config::block_size, config::window_size, config::future_limit,
                     config::max_matches, config::jobs, config::blocks,
                     config::print_progress, config::divisions,
                     config::prefix_lengths, config::hash_bits, config::finished,
                     config::processed_bytes.load(), config::total_bytes,
                     config::format, l, config::stats)
              << std::endl;
}

static auto get_mb() {
    std::ifstream stream("/proc/self/status");
    std::string   line;
    while (std::getline(stream, line)) {
        if (line.compare(0, 7, "VmPeak:") == 0) {
            size_t idx = line.find_first_of("0123456789");
            if (idx != std::string::npos) {
                unsigned long long vm_peak_kb = std::stoull(line.substr(idx));
                return vm_peak_kb / 1024;
            }
        }
    }
    return 0ULL;
}

static std::mutex mtx;
void              config::print_message(const std::string &message) {
    std::lock_guard<std::mutex> lock(mtx);
    static auto                 start_time = std::chrono::system_clock::now();
    auto                        now = std::chrono::system_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    if (!message.empty() && message[0] != '\r') {
        std::cout << "\r\33[2K" << "[" << elapsed << "s] " << message << std::flush;
    } else {
        std::cout << message << std::flush;
    }
}

static std::mutex              finish_mtx;
static std::condition_variable finish_cv;
void                           config::finish() {
    {
        std::lock_guard<std::mutex> lock(finish_mtx);
        config::finished = true;
    }
    finish_cv.notify_one();
}

void config::report_progress() {
    if (!print_progress)
        return;

    auto     start_time = std::chrono::system_clock::now();
    uint64_t last_processed = processed_bytes.load();

    auto format_size = [](uint64_t bytes) -> std::string {
        if (bytes >= 1024 * 1024 * 1024)
            return std::format("{:.2f} GB",
                               static_cast<double>(bytes) / (1024 * 1024 * 1024));
        if (bytes >= 1024 * 1024)
            return std::format("{:.2f} MB",
                               static_cast<double>(bytes) / (1024 * 1024));
        if (bytes >= 1024)
            return std::format("{:.2f} KB", static_cast<double>(bytes) / 1024);
        return std::format("{} B", bytes);
    };

    do {
        using namespace std::chrono_literals;
        auto interval_start_time = std::chrono::system_clock::now();

        std::unique_lock<std::mutex> lock(finish_mtx);
        if (finish_cv.wait_for(lock, 1000ms, [] { return finished; })) {
            break;
        }
        lock.unlock();

        auto now = std::chrono::system_clock::now();

        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time)
                .count();
        double interval_seconds =
            std::chrono::duration<double>(now - interval_start_time).count();

        auto     current_processed = processed_bytes.load();
        uint64_t interval_bytes = (current_processed >= last_processed)
                                      ? (current_processed - last_processed)
                                      : 0;
        last_processed = current_processed;

        double current_rate =
            (interval_seconds > 0)
                ? (static_cast<double>(interval_bytes) / interval_seconds)
                : 0.0;
        double mb_s = current_rate / (1024.0 * 1024.0);

        auto remaining_bytes = (total_bytes > current_processed)
                                   ? (total_bytes - current_processed)
                                   : 0;
        auto eta = (current_rate > 0)
                       ? static_cast<int64_t>(remaining_bytes / current_rate)
                       : 0;
        auto total_estimated = elapsed + eta;

        double percentage =
            (total_bytes > 0) ? (100.0 * current_processed / total_bytes) : 0.0;

        config::print_message(std::format(
            "\r[{}s] {:.1f}% ({} / {}, {:.2f} MB/s, "
            "Elapsed: {}s, Total: {}s, MEM: {}MB)",
            elapsed, percentage, format_size(current_processed),
            format_size(total_bytes), mb_s, elapsed, total_estimated, get_mb()));

    } while (!finished);

    config::print_message("\r\33[2KProgress complete.\n");
}

void config::apply_level(size_t file_size) {
    auto cores = static_cast<size_t>(std::thread::hardware_concurrency());

    if (level[0]) {
        block_size = file_size;
        window_size = file_size;
        future_limit = 256;
        max_matches = 20;
        divisions = cores;
        prefix_lengths = std::vector<uint32_t>{5};
        return;
    };

    if (level[1]) {
        block_size = file_size;
        window_size = file_size;
        future_limit = 256;
        max_matches = 200;
        divisions = cores;
        prefix_lengths = std::vector<uint32_t>{5};
        return;
    };

    if (level[2]) {
        block_size = file_size;
        window_size = file_size;
        future_limit = 256;
        max_matches = 20;
        divisions = cores;
        prefix_lengths = std::vector<uint32_t>{5, 6, 8};
        return;
    };

    if (level[3]) {
        block_size = file_size;
        window_size = file_size;
        future_limit = 256;
        max_matches = 200;
        divisions = cores;
        prefix_lengths = std::vector<uint32_t>{5, 6, 8};
        return;
    };

    if (level[4]) {
        block_size = file_size;
        window_size = file_size;
        future_limit = 256;
        max_matches = 20;
        divisions = cores;
        prefix_lengths = std::vector<uint32_t>{5, 6, 8, 12, 16, 20, 24};
        return;
    };

    if (level[5]) {
        block_size = file_size;
        window_size = file_size;
        future_limit = 256;
        max_matches = 200;
        divisions = cores;
        prefix_lengths = std::vector<uint32_t>{5, 6, 8, 12, 16, 20, 24};
        return;
    };

    if (level[6]) {
        block_size = file_size;
        window_size = file_size;
        future_limit = 256;
        max_matches = 2000;
        divisions = cores;
        prefix_lengths = std::vector<uint32_t>{5, 6, 8, 12, 16, 20, 24};
        return;
    };

    if (level[7]) {
        block_size = file_size;
        window_size = file_size;
        future_limit = 256;
        max_matches = 0;
        divisions = cores;
        prefix_lengths = std::vector<uint32_t>{5};
        return;
    };
}
