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
#include "div_encoder.h"
#include "encoder.h"
#include "formats.h"
#include "ioreader.h"
#include "iowriter.h"
#include "mp_encoder.h"
#include "token.h"
#include "worker_pool.h"

extern "C" {
int verbose;
}

template <typename Encoder>
static void encode(auto &in, auto &out, auto print_total_tokens) {
    auto format = formats::format::get_for_option(config::format);
    format.verify_config();

    auto   pool = worker_pool{config::jobs};
    size_t total = 0;

    auto printer = [&out, &format, &total](const std::vector<token> &tokens) {
        format.write_format_mark(out);
        format.write_block(tokens, out);
        total += tokens.size();
    };

    ioreader reader(in, config::blocks, config::block_size);
    iowriter writer(printer);

    std::jthread reader_thread{[&]() { reader.catchup_loop(); }};
    std::jthread writer_thread{[&]() { writer.catchup_loop(); }};

    std::vector<std::jthread> workers(config::jobs);
    for (size_t i = 0; i < config::jobs; i++) {
        workers[i] = std::jthread{[&] {
            while (true) {
                auto b = reader.get();
                if (b == std::nullopt) {
                    break;
                }

                Encoder encoder;

                auto [data_buffer, bytes_loaded] = encoder.for_loading();
                std::copy(b.value().data.begin(), b.value().data.end(), data_buffer);
                bytes_loaded = b.value().data.size();

                writer.put(b.value().index, std::move(encoder.encode()));
            }
        }};
    };

    for (auto &worker : workers) {
        worker.join();
    }
    reader_thread.join();
    writer.stop();
    writer_thread.join();

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
    app.add_option("-i", input_file, "Input file")->required();

    std::string output_file = "";
    app.add_option("-o", output_file, "Output file")->required();

    bool measure_time = false;
    app.add_flag("-m", measure_time, "Measure execution time");

    bool print_total_tokens = false;
    app.add_flag("-t", print_total_tokens,
                 "Print total tokens produced (if encoding)");

    app.add_flag("-p", config::print_progress, "Print progress");

    bool print_config = false;
    app.add_flag("-c", print_config, "Print config and exit");

    app.add_option("-l", config::level,
                   "Compression level (-1 = allow individual parameters, 1 = "
                   "fastest(default), 12 = very good compression in reasonable "
                   "time, 13-15 = best compression in unreasonably big time");

    app.add_option("-f", config::format,
                   "Output format (if encoding): readable, ctx(default)");
    app.add_option("-j", config::jobs, "Number of encoders to work in parallel");
    app.add_option("-b", config::blocks,
                   "Number of input blocks to read in parallel");
    app.add_option("--bs", config::block_size, "Processing block size in bytes");
    app.add_option("--ws", config::window_size, "Dictionary window size in bytes");
    app.add_option("--fl", config::future_limit, "Lookahead buffer limit size");
    app.add_option("--mm", config::max_matches, "Max matches before acceptation");
    app.add_option("--lm", config::lazy_matching,
                   "Bytes to skip during lazy matching");
    app.add_option(
        "-k", config::divisions,
        "Number of workers to process a single block. If 1, uses the standard "
        "encoder. If more, uses a different encoded that works best on big blocks");
    app.add_option(
        "--mpl", config::max_prefix_lengths,
        "How many prefix lengths to check. If 1, uses the standard encoder. If "
        "more, uses a different encoder that should be faster");

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

    config::apply_level(config::level, std::filesystem::file_size(
                                           std::filesystem::path{input_file}));
    if (print_config) {
        config::print();
        return 0;
    }
    if (config::max_prefix_lengths > mp_encoder::sizes.size()) {
        throw std::runtime_error(
            std::format("Too many prefix lengths: {}. Can only handle {}",
                        config::max_prefix_lengths, mp_encoder::sizes.size()));
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    if (run_encoder) {
        config::total_bytes =
            std::filesystem::file_size(std::filesystem::path{input_file});
        config::processed_bytes = 0;

        auto printer = std::jthread{config::report_progress};
        if (config::max_prefix_lengths != 1) {
            encode<mp_encoder>(in, out, print_total_tokens);
        } else if (config::divisions == 1) {
            encode<encoder>(in, out, print_total_tokens);
        } else {
            encode<div_encoder>(in, out, print_total_tokens);
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
