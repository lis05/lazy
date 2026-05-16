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

    std::string format_opt = "tokens";
    app.add_option("-f", format_opt, "Output format (if encoding)");

    bool measure_time = false;
    app.add_flag("-m", measure_time, "Measure execution time");

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

    auto format = formats::format::get_readable();

    auto start_time = std::chrono::high_resolution_clock::now();
    if (run_encoder) {
        encoder encoder;
        auto [data_buffer, bytes_loaded] = encoder.for_loading();

        while (true) {
            encoder.reset();
            in.read(reinterpret_cast<char *>(data_buffer), config::block_size);
            bytes_loaded = in.gcount();
            if (bytes_loaded == 0) {
                break;
            }
            auto tokens = encoder.encode();

            format.write_format_mark(out);
            format.write_block(tokens, out);
        }
    } else if (run_decoder) {
        decoder decoder;

        while (true) {

            uint8_t mark;
            in >> mark;
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
