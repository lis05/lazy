#include "config.h"

#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

std::vector<uint32_t> config::max_matches{20};
std::vector<uint32_t> config::prefix_lengths{5};
uint32_t              config::divisions = 1;
uint32_t              config::hash_bits = 0;
uint32_t              config::passes = 1;
uint32_t              config::threads = 1;

std::string config::load_hashchains = "";
std::string config::load_tokens = "";

std::string config::format = "main";
bool        config::use_turborc = false;
bool        config::use_turboans = false;
bool        config::use_fse = false;
bool        config::use_huf = false;
bool        config::use_memcpy = false;
bool        config::use_rans_static0 = false;
bool        config::use_rans_static1 = false;

bool     config::stats = false;
bool     config::metrics = false;
uint32_t config::verbosity = 0;

bool config::level[15] = {};

void config::print_config() {
    int l = 0;
    for (int i = 0; i < sizeof(config::level) / sizeof(config::level[0]); i++) {
        if (level[i])
            l = i;
    }
    std::cout << std::format("TODO") << std::endl;
}

static auto fmt(auto num) {
    if (num < 1000) {
        return std::format("{}", num);
    } else if (num <= 1000000) {
        return std::format("{:.1f}K", 1.0 * num / 1000);
    } else if (num <= 1000000000) {
        return std::format("{:.1f}M", 1.0 * num / 1000000);
    } else {
        return std::format("{:.1f}G", 1.0 * num / 1000000000);
    }
};

static auto get_memory_usage() {
    std::ifstream stream("/proc/self/status");
    std::string   line;
    while (std::getline(stream, line)) {
        if (line.compare(0, 7, "VmPeak:") == 0) {
            size_t idx = line.find_first_of("0123456789");
            if (idx != std::string::npos) {
                unsigned long long vm_peak_kb = std::stoull(line.substr(idx));
                return vm_peak_kb * 1024;
            }
        }
    }
    return 0ULL;
}

using s_clock = std::chrono::system_clock;
static std::mutex  mtx;
static std::string current_action = "";
static bool        current_action_has_counter = false;
static uint32_t    current_pass = 0;

void config::set_pass(uint32_t pass) {
    std::lock_guard<std::mutex> lock(mtx);
    current_pass = pass;
}

void config::start_action(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx);
    current_action = name;
    current_action_has_counter = false;
}

void config::start_action_with_counter(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx);
    current_action = name;
    current_action_has_counter = true;
}

static std::condition_variable finish_cv;
static bool                    finished = false;
void                           config::finish() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        finished = true;
    }
    finish_cv.notify_one();
}

void config::report_progress() {
    if (config::verbosity == 0) {
        return;
    }

    using namespace std::chrono_literals;
    decltype(1ms) sleep_interval;
    if (config::verbosity == 1) {
        sleep_interval = 2000ms;
    } else if (config::verbosity == 2) {
        sleep_interval = 500ms;
    } else {
        sleep_interval = 200ms;
    }

    static auto start = s_clock::now();

    do {
        std::unique_lock<std::mutex> lock(mtx);
        if (finish_cv.wait_for(lock, sleep_interval, [] { return finished; })) {
            break;
        }
        lock.unlock();

        auto now = s_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start);

        std::chrono::hh_mm_ss elapsed_split{elapsed};

        constexpr const char *CLEAR = "\r\33[2K";

#define TIME_FMT

        std::cout << CLEAR;
        if (!current_action_has_counter) {
            std::cout << std::format("[{} / {}, {:%H:%M:%S}] {}", current_pass + 1,
                                     config::passes, elapsed_split, current_action);
        } else {
            std::cout << std::format("[{} / {}, {:%H:%M:%S}] {} {:.2f}% ({} / {})",
                                     current_pass + 1, config::passes, elapsed_split,
                                     current_action,
                                     100.0 * counter.load() / max_counter,
                                     fmt(counter.load()), fmt(max_counter));
        }
        std::cout << "\n";
    } while (!finished);
}

