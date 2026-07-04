#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>

constexpr uint32_t NONE32 = 2e9 + 1000;
constexpr uint64_t INF64 = 2e18 + 1000;

class config {
public:
    static constexpr uint32_t    min_match_length = 5;
    static constexpr uint32_t    max_match_length = 255;
    static constexpr std::string version = "v1.1.0";

    static std::vector<uint32_t> prefix_lengths;
    static std::vector<uint32_t> max_matches;
    static std::vector<uint32_t> depth_limit_log;
    static std::vector<uint32_t> hash_bits;
    static uint32_t              blocks;
    static uint32_t              passes;
    static uint32_t              threads;

    static std::string format;
    static bool        use_turborc;
    static bool        use_turboans;
    static bool        use_fse;
    static bool        use_huf;
    static bool        use_memcpy;
    static bool        use_rans_static0;
    static bool        use_rans_static1;

    static std::string load_hashchains;
    static std::string load_tokens;

    static bool     stats;
    static bool     metrics;
    static uint32_t verbosity;

    static bool level[30];

    static std::atomic<uint64_t> counter;
    static uint64_t              max_counter;
    static void                  set_pass(uint32_t pass);
    static void                  start_action(const std::string &name);
    static void                  start_action_with_counter(const std::string &name);
    static void                  finish();
    static void                  print_config();
    static void                  report_progress();
    static void                  apply_level();
};
