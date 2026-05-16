#include <chrono>
#include <cstdio>
#include <iostream>

#include "CLI11.h"
#include "config.h"
#include "encoder.h"
#include "token.h"

int main(int argc, char **argv) {
    CLI::App app{"Simple LZ77 encoder."};
    argv = app.ensure_utf8(argv);

    std::string input_file = "";
    app.add_option("-i", input_file, "File to encode");

    std::string output_file = "";
    app.add_option("-o", output_file, "Output file");

    size_t window_size = 4096;
    app.add_option("-w", window_size, "Sliding window size");

    size_t history_size = 4080;
    app.add_option("-b", history_size,
                   "Size of the history buffer in the sliding window");

    size_t prefix_size = 3;
    app.add_option("-p", prefix_size, "Maximal size of a prefix to hash");

    size_t input_block_size = 64 * 1024;
    app.add_option("-B", input_block_size, "Size of the input block to buffer");

    bool output_tokens = false;
    app.add_flag("-t", output_tokens, "Output readable tokens");

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

    encoder encoder{config{window_size, history_size, prefix_size}};

    std::vector<std::byte> input_buffer(input_block_size);
    size_t                 bytes_buffered = 0, next_to_read = 0;
    auto loader = [&in, &input_buffer, input_block_size, &bytes_buffered,
                   &next_to_read](std::byte *data, size_t len) mutable -> size_t {
        size_t copied = 0;
        while (copied < len) {
            if (next_to_read == bytes_buffered) {
                in.read(reinterpret_cast<char *>(input_buffer.data()),
                        input_block_size);
                bytes_buffered = in.gcount();
                next_to_read = 0;
                if (bytes_buffered == 0) {
                    break;
                }
            }
            size_t to_copy = std::min(len - copied, bytes_buffered - next_to_read);
            std::memcpy(data + copied, input_buffer.data() + next_to_read, to_copy);
            next_to_read += to_copy;
            copied += to_copy;
        }
        return copied;
    };
    std::optional<token> t;
    bool                 end = false;

    auto start_time = std::chrono::high_resolution_clock::now();

    while (true) {
        t = encoder.encode(loader, end);
        if (t) {
            if (output_tokens) {
                if (std::holds_alternative<std::byte>(t.value())) {
                    out << (int)std::get<std::byte>(t.value()) << "\n";
                } else {
                    auto m = std::get<match>(t.value());
                    out << "(" << m.distance << ", " << m.length << ")"
                        << "\n";
                }
            }
        }
        if (end) {
            break;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_time = end_time - start_time;

    if (measure_time) {
        std::cout << "TIME: " << elapsed_time.count() << "s\n";
    }

    return 0;
}
