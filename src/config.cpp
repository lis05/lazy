#include "config.h"

#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
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

    std::cout << std::format("divisions={} passes={} threads={} hash_bits={}\n",
                             divisions, passes, threads, hash_bits);

    std::cout << std::format("prefix_lengths=[{}]\n", [&] {
        std::string s;
        for (size_t i = 0; i < prefix_lengths.size(); i++) {
            if (i)
                s += ',';
            s += std::to_string(prefix_lengths[i]);
        }
        return s;
    }());

    std::cout << std::format("max_matches=[{}]\n", [&] {
        std::string s;
        for (size_t i = 0; i < max_matches.size(); i++) {
            if (i)
                s += ',';
            s += std::to_string(max_matches[i]);
        }
        return s;
    }());

    std::cout << std::format("level={}\n", l);

    std::cout << std::format(
        "format={} turborc={} turboans={} fse={} huf={} "
        "memcpy={} rans0={} rans1={}\n",
        format, use_turborc, use_turboans, use_fse, use_huf, use_memcpy,
        use_rans_static0, use_rans_static1);

    if (!load_hashchains.empty())
        std::cout << std::format("load_hashchains={}\n", load_hashchains);
    if (!load_tokens.empty())
        std::cout << std::format("load_tokens={}\n", load_tokens);

    std::cout << std::format("stats={} metrics={} verbosity={}\n", stats, metrics,
                             verbosity);
}

static auto fmt_mem(auto num) {
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

static auto fmt_ms(auto ms) {
    if (ms < 1000) {
        return std::format("{}ms", ms);
    } else if (ms < 60 * 1000) {
        return std::format("{:.2f}s", ms / 1000.0);
    } else if (ms < 60 * 60 * 1000) {
        auto minutes = ms / 60000;
        ms -= minutes * 60000;
        return std::format("{}m {:.1f}s", minutes, ms / 1000.0);
    } else {
        auto hours = ms / 3600000;
        ms -= hours * 3600000;
        auto minutes = ms / 60000;
        ms -= minutes * 60000;
        return std::format("{}h {}m {:.1f}s", hours, minutes, ms / 1000.0);
    }
}

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

static std::mutex mtx;

std::atomic<uint64_t> config::counter = 0;
uint64_t              config::max_counter = 0;

struct action {
    std::string         name;
    s_clock::time_point started_at;
    s_clock::time_point ended_at;
};

static action   current_action{};
static bool     current_action_has_counter = false;
static uint32_t current_pass = 0;

static bool        previous_action_can_exist = false;
std::deque<action> previous_actions;

void config::set_pass(uint32_t pass) {
    std::lock_guard<std::mutex> lock(mtx);
    current_pass = pass;
}

void config::start_action(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx);

    if (previous_action_can_exist) {
        previous_actions.push_back(current_action);
        previous_actions.back().ended_at = s_clock::now();
    }
    previous_action_can_exist = true;

    current_action.name = name;
    current_action.started_at = s_clock::now();
    current_action_has_counter = false;
}

void config::start_action_with_counter(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx);

    if (previous_action_can_exist) {
        previous_actions.push_back(current_action);
        previous_actions.back().ended_at = s_clock::now();
    }
    previous_action_can_exist = true;

    current_action.name = name;
    current_action.started_at = s_clock::now();
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
        sleep_interval = 2000ms;
    } else {
        sleep_interval = 200ms;
    }

    static auto start = s_clock::now();

    do {
        constexpr const char *CLEAR = "\r\33[2K";

        std::unique_lock<std::mutex> lock(mtx);
        if (finish_cv.wait_for(lock, sleep_interval, [] { return finished; })) {
            if (config::verbosity >= 2) {
                auto now = s_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   now - start)
                                   .count();

                for (auto a : previous_actions) {
                    std::cout << CLEAR;
                    std::cout << std::format(
                        "[{} / {}, {}] {} | took {} at {} MEM\n", current_pass + 1,
                        config::passes, fmt_ms(elapsed), a.name,
                        fmt_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   a.ended_at - a.started_at)
                                   .count()),
                        fmt_mem(get_memory_usage()));
                }
            }
            break;
        }
        lock.unlock();

        auto now = s_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
                .count();

#define TIME_FMT

        if (config::verbosity >= 2) {
            for (auto a : previous_actions) {
                std::cout << CLEAR;
                std::cout << std::format(
                    "[{} / {}, {}] {} | took {} at {} MEM\n", current_pass + 1,
                    config::passes, fmt_ms(elapsed), a.name,
                    fmt_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
                               a.ended_at - a.started_at)
                               .count()),
                    fmt_mem(get_memory_usage()));
            }
            previous_actions.clear();
        }

        std::cout << CLEAR;
        if (!current_action_has_counter) {
            std::cout << std::format(
                "[{} / {}, {}] {} | {} at {} MEM", current_pass + 1, config::passes,
                fmt_ms(elapsed), current_action.name,
                fmt_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - current_action.started_at)
                           .count()),
                fmt_mem(get_memory_usage()));
        } else {
            std::cout << std::format(
                "[{} / {}, {}] {} {:.2f}% ({} / {}) | {} at {} MEM",
                current_pass + 1, config::passes, fmt_ms(elapsed),
                current_action.name, 100.0 * counter.load() / max_counter,
                fmt_mem(counter.load()), fmt_mem(max_counter),
                fmt_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - current_action.started_at)
                           .count()),
                fmt_mem(get_memory_usage()));
        }
        std::cout << std::flush;
    } while (!finished);

    std::cout << "\n";
}

