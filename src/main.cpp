#include <CLI11/include/CLI/CLI.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <latch>
#include <map>

#include "config.h"
#include "decoder.h"
#include "formats.h"
#include "ioreader.h"
#include "iowriter.h"
#include "mpo_encoder.h"
#include "token.h"
#include "worker_pool.h"

extern "C" {
int verbose;
}

template <typename Encoder>
static void encode(auto &in, auto &out, auto print_total_tokens, auto &pp) {
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

                config::print_message(
                    std::format("Processing block {}\n", b.value().index));
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

    config::finish();
    pp.join();

    if (print_total_tokens) {
        config::print_message(std::format("Total tokens: {}\n", total));
    }
}

static void decode(auto &in, auto &out, auto &pp) {
    decoder decoder;

    uint64_t i = 0;
    while (true) {
        unsigned char mark;
        if (!(in >> mark)) {
            break;
        }
        config::print_message(std::format("Processing block {}\n", i++));
        auto format = formats::format::get_for_mark(mark);
        auto [orig_size, tokens] = format.read_block(in);

        if (tokens.empty()) {
            break;
        }

        decoder.reset();
        decoder.decode(orig_size, tokens);
        auto [data, len] = decoder.get_bytes();
        out.write(reinterpret_cast<const char *>(data), len);
    }

    config::finish();
    pp.join();
}

class CleanFormatter : public CLI::Formatter {
public:
    CleanFormatter() : Formatter() {
        enable_option_defaults(false);
    }

    std::string make_option(const CLI::Option *opt,
                            bool               is_positional) const override {
        std::string output = Formatter::make_option(opt, is_positional);

        size_t pos = output.find(" [0]");
        if (pos != std::string::npos) {
            output.erase(pos, 4);
        }
        return output;
    }

    std::string make_group(std::string group, bool is_positional,
                           std::vector<const CLI::Option *> opts) const override {
        std::string output = Formatter::make_group(group, is_positional, opts);
        if (!output.empty()) {
            output += "\n";
        }
        return output;
    }
};

int main(int argc, char **argv) {
    CLI::App app{"lzmpo: LZ77 multiple prefixes optimal compressor"};
    app.formatter(std::make_shared<CleanFormatter>());
    argv = app.ensure_utf8(argv);

    auto main_group = app.add_option_group("Input/output files and running mode");
    bool run_encoder = false;
    bool run_decoder = false;
    std::string input_file = "";
    std::string output_file = "";
    main_group->add_flag("-e", run_encoder, "Run encoder");
    main_group->add_flag("-d", run_decoder, "Run decoder");
    main_group->add_option("-i", input_file, "Input file")->required();
    main_group->add_option("-o", output_file, "Output file")->required();

    auto level_group = app.add_option_group(
        "Compression levels",
        "Each subsequent level results in slower runtime, better compression ratio, "
        "and greater memory usage than the previous level");
    level_group->add_flag("-0", config::level[0],
                          "Same as --fit --mm 20 -k<supported threads> --pl 5");
    level_group->add_flag("-1", config::level[1],
                          "Same as --fit --mm 200 -k<supported threads> --pl 5");
    level_group->add_flag("-2", config::level[2],
                          "Same as --fit --mm 20 -k<supported threads> --pl 5,6,8");
    level_group->add_flag("-3", config::level[3],
                          "Same as --fit --mm 200 -k<supported threads> --pl 5,6,8");
    level_group->add_flag(
        "-4", config::level[4],
        "Same as --fit --mm 20 -k<supported threads> --pl 5,6,8,12,16,20,24");
    level_group->add_flag(
        "-5", config::level[5],
        "Same as --fit --mm 200 -k<supported threads> --pl 5,6,8,12,16,20,24");
    level_group->add_flag(
        "-6", config::level[6],
        "Same as --fit --mm 2000 -k<supported threads> --pl 5,6,8,12,16,20,24");
    level_group->add_flag("-7", config::level[7],
                          "Same as --fit --mm 0 -k<supported threads> --pl 5");
    level_group->add_flag("-8", config::level[8],
                          "Same as --fit --mm 0 -k<supported threads> --pl 5 -b32");

    auto advanced_group = app.add_option_group("Advanced flags");
    bool set_max_windows = false;
    advanced_group->add_option("--bs", config::block_size,
                               "Processing block size in bytes");
    advanced_group->add_option("--ws", config::window_size,
                               "Dictionary window size in bytes");
    advanced_group->add_option("--fl", config::future_limit,
                               "Lookahead buffer limit size");
    advanced_group->add_flag("--fit", set_max_windows,
                             "Set --bs,--ws,--fl to fit the entire file");
    advanced_group->add_option("--mm", config::max_matches,
                               "Max matches to check for each prefix length");
    advanced_group->add_option("-k", config::divisions,
                               "Number of threads to process a single block");
    advanced_group
        ->add_option("--pl", config::prefix_lengths,
                     "Comma-separated list of prefix lengths")
        ->delimiter(',');
    advanced_group->add_option(
        "-b", config::hash_bits,
        "Bits per hash. If 0, a hash map will be used. If not "
        "0, a table of size 2^<bits> will "
        "be used. Should not exceed 32");

    auto misc_group = app.add_option_group("Miscellaneous");
    bool measure_time = false;
    bool print_total_tokens = false;
    bool print_config = false;
    misc_group->add_flag("-m", measure_time, "Measure execution time");

    misc_group->add_flag("-t", print_total_tokens,
                         "Print total tokens produced (if encoding)");
    misc_group->add_flag("-p", config::print_progress, "Print progress");
    misc_group->add_flag("-c", print_config, "Print config and exit");
    misc_group->add_flag("-s", config::stats,
                         "Calculate various statistics and write them to ./stats/");
    misc_group->add_option(
        "--lhc", config::load_hashchains,
        "Load & sync hashchains to file. Can speed up on subsequent runs");

    CLI11_PARSE(app, argc, argv);

    std::ifstream in(input_file, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Failed to read from " << input_file << std::endl;
        return -1;
    }
    auto input_file_size = std::filesystem::file_size(input_file);

    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Failed to write to " << input_file << std::endl;
        return -1;
    }

    if (set_max_windows) {
        config::block_size = config::window_size = input_file_size;
        config::future_limit = 256;
    }

    if (config::hash_bits > 32) {
        std::cerr << "Invalid number of hash bits" << std::endl;
    }

    config::apply_level(input_file_size);

    if (print_config) {
        config::print();
        return 0;
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    if (run_encoder) {
        auto printer = std::jthread{config::report_progress};
        encode<mpo_encoder>(in, out, print_total_tokens, printer);
    } else if (run_decoder) {
        auto printer = std::jthread{config::report_progress};
        decode(in, out, printer);
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_time = end_time - start_time;

    if (measure_time) {
        std::cout << std::format("TIME: {}s\n", elapsed_time.count());
    }

    in.close();
    out.close();

    return 0;
}
