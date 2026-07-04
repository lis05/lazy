#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <CLI11/include/CLI/CLI.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <latch>
#include <map>

#include "bins.h"
#include "config.h"
#include "decoder.h"
#include "encoder.h"
#include "estimators.h"
#include "formats.h"
#include "token.h"
#include "worker_pool.h"

extern "C" {
int verbose;
}

static std::pair<std::byte *, int> mmap_file(const std::string &filename,
                                             bool as_input, uint32_t size) {
    if (as_input) {
        int in = open(filename.c_str(), O_RDONLY);
        if (in == -1) {
            throw std::runtime_error("Failed to open " + filename);
        }

        std::byte *ptr = static_cast<std::byte *>(
            mmap(nullptr, size, PROT_READ, MAP_PRIVATE, in, 0));
        if (ptr == nullptr) {
            throw std::runtime_error("Failed to mmap " + filename);
        }

        return std::pair{ptr, in};
    } else {
        int out = open(filename.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (out == -1 || ftruncate(out, size) == -1) {
            throw std::runtime_error("Failed to open the output file");
        }

        std::byte *ptr = static_cast<std::byte *>(
            mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, out, 0));
        if (ptr == nullptr) {
            throw std::runtime_error("Failed to mmap " + filename);
        }

        return std::pair{ptr, out};
    }
}

template <typename Encoder>
static void encode(const std::string filename_in, const std::string filename_out,
                   auto &pp) {
    uint32_t filesize_in = std::filesystem::file_size(filename_in);
    uint32_t filesize_out = filesize_in;

    auto [ptr_in, fd_in] = mmap_file(filename_in, true, filesize_in);
    auto [ptr_out, fd_out] = mmap_file(filename_out, false, filesize_out);

    estimators::smart  est;
    std::vector<token> tokens;
    Encoder            encoder;

    encoder.load(ptr_in, filesize_in);

    for (uint32_t pass = 0; pass < config::passes; pass++) {
        config::set_pass(pass);

        tokens = encoder.encode(pass, est);
        encoder.reset_for_next_pass(pass);
    }

    auto       format = formats::format::get_for_option(config::format);
    std::byte *end_ptr = format.write_format_mark(ptr_out);
    end_ptr = format.write_block(tokens, end_ptr, filesize_out);

    if (end_ptr - ptr_out > filesize_out) {
        throw std::runtime_error("File is incompressible");
    }

    munmap(ptr_out, filesize_out);
    filesize_out = end_ptr - ptr_out;
    munmap(ptr_in, filesize_in);

    if (ftruncate(fd_out, filesize_out) == -1) {
        throw std::runtime_error("Encode failed");
    }

    config::finish();
    pp.join();

    close(fd_in);
    close(fd_out);
}

static void decode(const std::string &filename_in, const std::string &filename_out,
                   auto &pp) {
    decoder decoder;

    auto in_size = std::filesystem::file_size(filename_in);
    auto [ptr_in, in] = mmap_file(filename_in, true, in_size);

    unsigned char mark = static_cast<unsigned char>(*ptr_in);
    auto          format = formats::format::get_for_mark(mark);
    auto [orig_size, streams] = format.read_block(ptr_in + 1);

    auto [ptr_out, out] = mmap_file(filename_out, false, orig_size);
    decoder.decode(orig_size, ptr_out, streams);

    std::free(streams.controls);
    std::free(streams.literals);
    std::free(streams.distances);
    std::free(streams.lengths);
    munmap(ptr_out, orig_size);
    munmap(ptr_in, in_size);

    if (ftruncate(out, orig_size) == -1) {
        throw std::runtime_error("Failed to shrink the output file");
    }
    close(out);
    close(in);

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
    uint32_t cores = std::thread::hardware_concurrency();
    dist_bins::precalc();

    CLI::App app("LZ77 multiple prefixes optimal compressor",
                 std::string("lzmpo ") + config::version);
    app.formatter(std::make_shared<CleanFormatter>());
    argv = app.ensure_utf8(argv);

    bool        run_encoder = false;
    bool        run_decoder = false;
    std::string input_file = "input.lzmpo";
    std::string output_file = "output.lzmpo";
    app.add_flag("-e", run_encoder, "Run encoder");
    app.add_flag("-d", run_decoder, "Run decoder");
    app.add_option("-i", input_file, "Input file");
    app.add_option("-o", output_file, "Output file");

    app.add_option("--pl", config::prefix_lengths,
                   "Comma-separated list of prefix lengths")
        ->delimiter(',');
    app.add_option("--mm", config::max_matches,
                   "Comma-separated list of max matches per prefix length")
        ->delimiter(',');
    app.add_option(
           "--dl", config::depth_limit_log,
           "Comma-separated list of depth limit per prefix length (NUM[G|M|K])")
        ->delimiter(',');
    app.add_option("--hb", config::hash_bits,
                   "Comma-separated list of hash bits per prefix length")
        ->delimiter(',');

    app.add_option("-k", config::blocks, "Divide input into blocks");
    app.add_option("-a", config::passes, "How many passes to do");
    app.add_option("-T", config::threads, "How many threads to use");

    app.add_flag("--turborc", config::use_turborc, "Use TurboRC as backend");
    app.add_flag("--turboans", config::use_turboans, "Use TurboANS as backend");
    app.add_flag("--fse", config::use_fse,
                 "Use finitestateentropy's fse as backend");
    app.add_flag("--huf", config::use_huf,
                 "Use finitestateentropy's huf as backend");
    app.add_flag("--memcpy", config::use_memcpy, "Use memcpy as backend");
    app.add_flag("--rans_static0", config::use_rans_static0,
                 "Use rans_static's r32x16b_avx2 order0 as backend");
    app.add_flag("--rans_static1", config::use_rans_static1,
                 "Use rans_static's r32x16b_avx2 order1 as backend");

    bool print_config = false;
    bool print_version = false;
    app.add_flag("-v", config::verbosity, "Verbosity level");
    app.add_flag("-c", print_config, "Print config and exit");
    app.add_flag("--stats", config::stats,
                 "Calculate various statistics and write them to ./stats/");
    app.add_flag("-m", config::metrics, "Prints compression metrics");
    app.add_option(
        "--lhc", config::load_hashchains,
        "Load & sync hashchains to file. Can speed up on subsequent runs");
    app.add_option("--lt", config::load_tokens,
                   "Load & sync tokens to file. Completly omits running the parser");
    app.add_flag("-V", print_version, "Print version");

    CLI11_PARSE(app, argc, argv);
    if (print_version) {
        std::cout << config::version << "\n";
        return 0;
    }

    if (!run_decoder && !run_encoder) {
        std::cout << "-i nor -d selected, no action taken\n";
        return 0;
    }

    if (std::filesystem::file_size(input_file) >= 2e9) {
        std::cerr << "Cannot work with files larger than 2e9 bytes\n";
        return -1;
    }

    if (!config::use_turborc && !config::use_turboans && !config::use_fse &&
        !config::use_huf && !config::use_memcpy && !config::use_rans_static0 &&
        !config::use_rans_static1) {
        config::use_rans_static0 = true;
    }

    for (auto hb : config::hash_bits) {
        if (hb == 0 || hb > 32) {
            std::cerr << "Invalid number of hash bits" << std::endl;
            return -1;
        }
    }

    config::apply_level();

    if (config::prefix_lengths.size() != config::max_matches.size() ||
        config::prefix_lengths.size() != config::depth_limit_log.size() ||
        config::prefix_lengths.size() != config::hash_bits.size()) {
        std::cerr << "Inconsistent number of parameters --pl --mm --dl --hb"
                  << std::endl;
        return -1;
    }

    if (print_config) {
        config::print_config();
        return 0;
    }

    config::start_action("Initializing");
    if (run_encoder) {
        auto printer = std::jthread{config::report_progress};
        encode<encoder>(input_file, output_file, printer);
    } else if (run_decoder) {
        auto printer = std::jthread{config::report_progress};
        config::passes = 1;
        decode(input_file, output_file, printer);
    }

    return 0;
}
