#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <latch>
#include <map>

#include "CLI11.h"
#include "config.h"
#include "decoder.h"
#include "encoder.h"
#include "formats.h"
#include "optimal_encoder.h"
#include "token.h"
#include "worker_pool.h"

template <typename Encoder>
static void encode(auto &in, auto &out, auto format_opt, auto print_total_tokens) {
    auto format = formats::format::get_for_option(format_opt);
    format.verify_config();

    auto   pool = worker_pool{config::jobs};
    size_t total = 0;

    while (in) {
        std::vector<std::vector<token>> batch_results(config::jobs);
        std::latch                      work_done(config::jobs);

        for (size_t i = 0; i < config::jobs; ++i) {
            auto buffer = std::make_shared<char[]>(config::block_size);
            in.read(buffer.get(), config::block_size);
            size_t bytes = in.gcount();

            if (bytes == 0) {
                for (size_t ii = i; ii < config::jobs; ii++) work_done.count_down();
                break;
            }

            pool.enqueue([&, buffer = std::move(buffer), i, bytes] {
                Encoder encoder;
                auto [data_buffer, bytes_loaded] = encoder.for_loading();

                memcpy(data_buffer, buffer.get(), bytes);
                bytes_loaded = bytes;

                batch_results[i] = std::move(encoder.encode());
                work_done.count_down();
            });
        }

        work_done.wait();

        for (const auto &tokens : batch_results) {
            if (tokens.empty())
                continue;
            format.write_format_mark(out);
            format.write_block(tokens, out);
            total += tokens.size();
        }
    }

    if (print_total_tokens) {
        std::cout << "Total tokens: " << total << "\n";
    }
}

static void decode(auto &in, auto &out) {
    decoder decoder;

    while (true) {
        unsigned char mark;
        if (!(in >> mark)) {
            break;
        }
        auto format = formats::format::get_for_mark(mark);
        auto tokens = format.read_block(in);

        if (tokens.empty()) {
            break;
        }

        decoder.reset();
        decoder.decode(tokens);
        auto [data, len] = decoder.get_bytes();
        out.write(reinterpret_cast<const char *>(data), len);
    }
}

int main(int argc, char **argv) {
    CLI::App app{"Simple LZ77 encoder."};
    argv = app.ensure_utf8(argv);

    bool run_encoder = false;
    app.add_flag("-e", run_encoder, "Run encoder");

    bool run_decoder = false;
    app.add_flag("-d", run_decoder, "Run decoder");

    std::string input_file = "";
    app.add_option("-i", input_file, "Input file");

    std::string output_file = "";
    app.add_option("-o", output_file, "Output file");

    std::string format_opt = "fse";
    app.add_option("-f", format_opt, "Output format (if encoding)");

    bool measure_time = false;
    app.add_flag("-m", measure_time, "Measure execution time");

    bool print_total_tokens = false;
    app.add_flag("-t", print_total_tokens,
                 "Print total tokens produced (if encoding)");

    bool print_progress = config::print_progress;
    app.add_flag("-p", print_progress, "Print progress");

    size_t jobs = config::jobs;
    app.add_option("-j", jobs, "Jobs for encoding");

    bool run_optimal = false;
    app.add_flag("-O", run_optimal, "Run optimal encoder instead of the greedy one");

    size_t block_size = config::block_size;
    size_t window_size = config::window_size;
    size_t future_limit = config::future_limit;
    size_t max_matches = config::max_matches;
    bool   lazy_matching = config::lazy_matching;

    app.add_option("--block-size", block_size,
                   "LZ77 processing block size in bytes");
    app.add_option("--window-size", window_size,
                   "LZ77 dictionary window size in bytes");
    app.add_option("--future-limit", future_limit,
                   "LZ77 lookahead buffer limit size");
    app.add_option("--max-matches", max_matches,
                   "LZ77 max matches before acceptation");
    app.add_flag("--lazy-matching", lazy_matching, "Do lazy matching");

    CLI11_PARSE(app, argc, argv);

    std::ifstream in(input_file, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Failed to read from " << input_file << std::endl;
        return -1;
    }

    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Failed to write to " << input_file << std::endl;
        return -1;
    }

    config::load(block_size, window_size, future_limit, max_matches, jobs,
                 print_progress, lazy_matching);
    auto start_time = std::chrono::high_resolution_clock::now();
    if (run_encoder) {
        config::total_bytes =
            std::filesystem::file_size(std::filesystem::path{input_file});
        config::processed_bytes = 0;

        auto printer = std::jthread{config::report_progress};
        if (run_optimal) {
            encode<optimal_encoder>(in, out, format_opt, print_total_tokens);
        } else {
            encode<encoder>(in, out, format_opt, print_total_tokens);
        }
    } else if (run_decoder) {
        decode(in, out);
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_time = end_time - start_time;

    if (measure_time) {
        std::cout << "TIME: " << elapsed_time.count() << "s\n";
    }

    in.close();
    out.close();

    return 0;
}
