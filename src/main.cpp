#include <chrono>
#include <cstdio>
#include <iostream>

#include "CLI11.h"
#include "config.h"
#include "decoder.h"
#include "encoder.h"
#include "formats.h"
#include "token.h"

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

    size_t block_size = config::block_size;
    size_t window_size = config::window_size;
    size_t future_limit = config::future_limit;
    size_t max_matches = config::max_matches;

    app.add_option("--block-size", block_size,
                   "LZ77 processing block size in bytes");
    app.add_option("--window-size", window_size,
                   "LZ77 dictionary window size in bytes");
    app.add_option("--future-limit", future_limit,
                   "LZ77 lookahead buffer limit size");
    app.add_option("--max-matches", max_matches,
                   "LZ77 max matches before acceptation");

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

    config::load(block_size, window_size, future_limit, max_matches);

    auto format = formats::format::get_for_option(format_opt);
    format.verify_config();

    auto start_time = std::chrono::high_resolution_clock::now();
    if (run_encoder) {
        encoder encoder;
        auto [data_buffer, bytes_loaded] = encoder.for_loading();

        size_t total = 0;

        while (true) {
            encoder.reset();
            in.read(reinterpret_cast<char *>(data_buffer), config::block_size);
            bytes_loaded = in.gcount();
            if (bytes_loaded == 0) {
                break;
            }
            auto tokens = encoder.encode();
            total += tokens.size();

            format.write_format_mark(out);
            format.write_block(tokens, out);
        }

        if (print_total_tokens) {
            std::cout << "Total tokens: " << total << "\n";
        }
    } else if (run_decoder) {
        decoder decoder;

        while (true) {
            unsigned char mark;
            if (!(in >> mark)) {
                break;
            }
            format = formats::format::get_for_mark(mark);
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
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_time = end_time - start_time;

    if (measure_time) {
        std::cout << "TIME: " << elapsed_time.count() << "s\n";
    }

    in.close();
    out.close();

    return 0;
}
