#include "mpo_encoder.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <latch>
#include <mutex>
#include <thread>

#include "bins.h"
#include "split.h"
#include "worker_pool.h"

mpo_encoder::mpo_encoder() : bytes_loaded(0), data(config::block_size), head(1) {
}

std::pair<std::byte *, size_t &> mpo_encoder::for_loading() {
    return {data.data(), bytes_loaded};
}

struct saved_hashchains {
    uint32_t                           data_checksum;
    std::vector<uint32_t>              prefix_lengths;
    std::vector<std::vector<uint32_t>> prev;

    friend auto &operator<<(auto &out, const saved_hashchains &h) {
        out.write(reinterpret_cast<const char *>(&h.data_checksum),
                  sizeof(h.data_checksum));

        uint64_t pl_size = static_cast<uint64_t>(h.prefix_lengths.size());
        out.write(reinterpret_cast<const char *>(&pl_size), sizeof(pl_size));
        out.write(reinterpret_cast<const char *>(h.prefix_lengths.data()),
                  pl_size * sizeof(uint32_t));

        uint64_t prev_size = static_cast<uint64_t>(h.prev.size());
        out.write(reinterpret_cast<const char *>(&prev_size), sizeof(prev_size));
        for (const auto &p : h.prev) {
            uint64_t p_size = static_cast<uint64_t>(p.size());
            out.write(reinterpret_cast<const char *>(&p_size), sizeof(p_size));
            out.write(reinterpret_cast<const char *>(p.data()),
                      p_size * sizeof(uint32_t));
        }
        return out;
    }

    friend auto &operator>>(auto &in, saved_hashchains &h) {
        in.read(reinterpret_cast<char *>(&h.data_checksum), sizeof(h.data_checksum));

        uint64_t pl_size = 0;
        in.read(reinterpret_cast<char *>(&pl_size), sizeof(pl_size));
        h.prefix_lengths.resize(pl_size);
        in.read(reinterpret_cast<char *>(h.prefix_lengths.data()),
                pl_size * sizeof(uint32_t));

        uint64_t prev_size = 0;
        in.read(reinterpret_cast<char *>(&prev_size), sizeof(prev_size));
        h.prev.resize(prev_size);
        for (auto &p : h.prev) {
            uint64_t p_size = 0;
            in.read(reinterpret_cast<char *>(&p_size), sizeof(p_size));
            p.resize(p_size);
            in.read(reinterpret_cast<char *>(p.data()), p_size * sizeof(uint32_t));
        }
        return in;
    }
};

bool mpo_encoder::load_hashchains() {
    if (config::load_hashchains.empty()) {
        return false;
    }

    config::print_message(std::format(
        "Loading hashchains from {} (may take a while)\n", config::load_hashchains));
    config::processed_bytes = 0;
    config::total_bytes = 1;
    std::ifstream in(config::load_hashchains, std::ios::binary);
    if (!in) {
        config::print_message("Hashchains file not found or cannot be opened.\n");
        return false;
    }

    saved_hashchains h;
    in >> h;

    if (in.fail() && !in.eof()) {
        throw std::runtime_error("load_hashchains: File read error");
    }

    uint32_t current_checksum =
        static_cast<uint32_t>(hashes::hashn(data.data(), bytes_loaded));
    if (h.data_checksum != current_checksum ||
        !std::ranges::equal(h.prefix_lengths, config::prefix_lengths)) {
        config::print_message(
            "Hashchains checksum or prefix lengths mismatch. Regenerating "
            "chains.\n");
        return false;
    }

    this->prev = std::move(h.prev);
    config::print_message("Hashchains loaded successfully.\n");
    return true;
}

void mpo_encoder::sync_hashchains() {
    if (config::load_hashchains.empty()) {
        return;
    }

    uint32_t current_checksum =
        static_cast<uint32_t>(hashes::hashn(data.data(), bytes_loaded));

    std::ifstream in(config::load_hashchains, std::ios::binary);
    if (in) {
        uint32_t read_checksum = 0;
        in.read(reinterpret_cast<char *>(&read_checksum), sizeof(read_checksum));

        uint64_t pl_size = 0;
        in.read(reinterpret_cast<char *>(&pl_size), sizeof(pl_size));
        std::vector<uint32_t> read_prefix_lengths(pl_size);
        in.read(reinterpret_cast<char *>(read_prefix_lengths.data()),
                pl_size * sizeof(uint32_t));

        if (!in.fail() && read_checksum == current_checksum &&
            std::ranges::equal(read_prefix_lengths, config::prefix_lengths)) {
            return;
        }
    }
    in.close();

    config::print_message(std::format("Saving hashchains to {} (may take a while)\n",
                                      config::load_hashchains));
    config::processed_bytes = 0;
    config::total_bytes = 1;
    std::ofstream out(config::load_hashchains, std::ios::binary);
    if (!out) {
        throw std::runtime_error("sync_hashchains: Failed to open file for writing");
    }

    out.write(reinterpret_cast<const char *>(&current_checksum),
              sizeof(current_checksum));

    uint64_t pl_size = static_cast<uint64_t>(config::prefix_lengths.size());
    out.write(reinterpret_cast<const char *>(&pl_size), sizeof(pl_size));
    out.write(reinterpret_cast<const char *>(config::prefix_lengths.data()),
              pl_size * sizeof(uint32_t));

    uint64_t prev_size = static_cast<uint64_t>(this->prev.size());
    out.write(reinterpret_cast<const char *>(&prev_size), sizeof(prev_size));
    for (const auto &p : this->prev) {
        uint64_t p_size = static_cast<uint64_t>(p.size());
        out.write(reinterpret_cast<const char *>(&p_size), sizeof(p_size));
        out.write(reinterpret_cast<const char *>(p.data()),
                  p_size * sizeof(uint32_t));
    }

    if (!out) {
        throw std::runtime_error("sync_hashchains: Failed to write data");
    }
    config::print_message("Hashchains saved successfully.\n");
}

void mpo_encoder::process(auto future_limit, const auto NONE, auto i,
                          auto *subblock_ptr) {
    const auto &data_buf = data.data();

    for (int len = config::prefix_lengths.size() - 1; len >= 0; len--) {
        const auto prefix_len = config::prefix_lengths[len];
        if (prefix_len > future_limit || i >= prev[len].size()) {
            continue;
        }

        if (len + 1 != config::prefix_lengths.size() &&
            future_limit > config::prefix_lengths[len + 1]) {
            future_limit = config::prefix_lengths[len + 1] - 1;
        }

        const auto &prev_buf = prev[len].data();

        uint64_t count = config::max_matches;
        for (uint32_t pos = prev_buf[i];
             pos != NONE && pos + config::window_size >= i && --count > 0;
             pos = prev_buf[pos]) {
            uint32_t match_len = strmatch::match_simd_loop(
                future_limit, data_buf + pos, data_buf + i);
            if (subblock_ptr[match_len] == NONE || subblock_ptr[match_len] < pos) {
                subblock_ptr[match_len] = pos;
            }
            if (match_len == future_limit) {
                break;
            }
        }
    }
}

std::vector<token> mpo_encoder::encode(uint32_t pass, estimators::smart &est) {
    constexpr auto INF = std::numeric_limits<uint64_t>::max();
    constexpr auto NONE = std::numeric_limits<uint32_t>::max();

    auto data_ptr = data.data();

    worker_pool pool(config::divisions);

    if (!load_hashchains()) {
        hashes.resize(bytes_loaded + 1);
        if (config::hash_bits == 0) {
            prev.resize(config::prefix_lengths.size());

            for (int len = config::prefix_lengths.size() - 1; len >= 0; len--) {
                const auto prefix_len = config::prefix_lengths[len];
                config::print_message(std::format(
                    "Calculating hashes for prefix_len = {}\n", prefix_len));
                config::total_bytes = bytes_loaded + 1;
                config::processed_bytes = 0;

                if (bytes_loaded < prefix_len) {
                    throw std::runtime_error("Cannot compress");
                }

                auto       blocks = split_range(bytes_loaded - prefix_len + 1,
                                                config::divisions, 1, bytes_loaded);
                std::latch finished(blocks.size());
                auto      *hashes_ptr = hashes.data();
                auto      *data_ptr = data.data();

                for (auto [start, end] : blocks) {
                    pool.enqueue([&, start, end]() mutable {
                        while (start <= end) {
                            hashes_ptr[start] =
                                hashes::hashn(data_ptr + start, prefix_len);
                            start++;
                            config::processed_bytes++;
                        }
                        finished.count_down();
                    });
                }

                finished.wait();

                config::print_message(std::format(
                    "Calculating hash chain for prefix_len = {}\n", prefix_len));
                config::total_bytes = bytes_loaded + 1;
                config::processed_bytes = 0;

                auto &pr = prev[len];
                pr.resize(bytes_loaded - prefix_len + 1);
                auto *pr_ptr = pr.data();
                for (size_t i = 0; i + prefix_len < bytes_loaded + 1; i++) {
                    auto h = hashes_ptr[i];
                    auto it = head_gp.find(h);
                    if (it == head_gp.end()) {
                        pr_ptr[i] = NONE;
                    } else {
                        pr_ptr[i] = it->second;
                    }
                    head_gp[h] = i;
                    config::processed_bytes++;
                }
                head_gp.clear();
            }
            using T = decltype(head_gp);
            T{}.swap(head_gp);
        } else {
            prev.resize(config::prefix_lengths.size());
            head = table{config::hash_bits};

            for (int len = config::prefix_lengths.size() - 1; len >= 0; len--) {
                const auto prefix_len = config::prefix_lengths[len];
                config::print_message(std::format(
                    "Calculating hashes for prefix_len = {}\n", prefix_len));

                if (bytes_loaded < prefix_len) {
                    throw std::runtime_error("Cannot compress");
                }

                config::total_bytes = bytes_loaded + 1;
                config::processed_bytes = 0;

                auto       blocks = split_range(bytes_loaded - prefix_len + 1,
                                                config::divisions, 1, bytes_loaded);
                std::latch finished(blocks.size());
                auto      *hashes_ptr = hashes.data();
                auto      *data_ptr = data.data();

                for (auto [start, end] : blocks) {
                    pool.enqueue([&, start, end]() mutable {
                        while (start <= end) {
                            hashes_ptr[start] =
                                hashes::hashn(data_ptr + start, prefix_len);
                            start++;
                            config::processed_bytes++;
                        }
                        finished.count_down();
                    });
                }

                finished.wait();

                config::print_message(std::format(
                    "Calculating hash chain for prefix_len = {}\n", prefix_len));
                config::total_bytes = bytes_loaded + 1;
                config::processed_bytes = 0;

                auto &pr = prev[len];
                pr.resize(bytes_loaded - prefix_len + 1);
                auto *pr_ptr = pr.data();
                for (size_t i = 0; i + prefix_len < bytes_loaded + 1; i++) {
                    auto h = hashes_ptr[i];
                    if (!head.has(h)) {
                        pr_ptr[i] = NONE;
                    } else {
                        pr_ptr[i] = head.get(h);
                    }
                    head.insert(h, i);
                    config::processed_bytes++;
                }
                head.clear();
            }
            head.destroy();
        }

        {
            using T = decltype(hashes);
            T{}.swap(hashes);
        }

        sync_hashchains();
    }

    write_stats_hashes(est);

    config::print_message(std::format("Calculating tokens + dp\n"));
    config::total_bytes = bytes_loaded + 1;
    config::processed_bytes = 0;

    auto blocks = split_range(bytes_loaded, config::divisions, 1, bytes_loaded);
    std::latch finished(blocks.size());

    dp_cost.resize(blocks.size());
    dp_from.resize(blocks.size());
    dp_pos.resize(blocks.size());
    states.resize(bytes_loaded + 1, estimators::state{0, 0, 0});

    size_t block_index = 0;
    for (auto [start, end] : blocks) {
        pool.enqueue([&, start, end, block_index]() mutable {
            end++;
            dp_cost[block_index].resize(end - start + 1, INF);
            dp_from[block_index].resize(end - start + 1, NONE);
            dp_pos[block_index].resize(end - start + 1, NONE);

            auto *dp_cost_ptr = dp_cost[block_index].data();
            auto *dp_from_ptr = dp_from[block_index].data();
            auto *dp_pos_ptr = dp_pos[block_index].data();

            std::vector<uint32_t> subblock(config::future_limit + 1, NONE);
            auto                  subblock_ptr = subblock.data();

            int i = start;

            dp_cost_ptr[i - start] = 0;
            dp_from_ptr[i - start] = start;
            dp_pos_ptr[i - start] = NONE;
            while (i < end) {
                auto future_limit =
                    std::min(static_cast<size_t>(end - i), config::future_limit);
                std::fill(subblock.begin(), subblock.end(), NONE);
                process(future_limit, NONE, i, subblock_ptr);

                auto cur_dp_cost_ptr = dp_cost_ptr[i - start];
                for (uint32_t match_len = future_limit; match_len >= 5;
                     match_len--) {
                    if (match_len != future_limit &&
                        subblock_ptr[match_len + 1] != NONE &&
                        (subblock_ptr[match_len] == NONE ||
                         subblock_ptr[match_len] < subblock_ptr[match_len + 1])) {
                        subblock_ptr[match_len] = subblock_ptr[match_len + 1];
                    }

                    if (subblock_ptr[match_len] == NONE) {
                        continue;
                    }

                    double edge_cost = est.match_cost(i - subblock_ptr[match_len],
                                                      match_len, states[i]);
                    double literals_cost = 0;
                    for (size_t ii = 0; ii < match_len; ii++) {
                        literals_cost += est.literal_cost(static_cast<uint64_t>(
                            data_ptr[subblock_ptr[match_len] + ii]));
                    }
                    // edge
                    if (edge_cost < literals_cost && i + match_len < end) {
                        double new_cost = cur_dp_cost_ptr + edge_cost;
                        if (dp_cost_ptr[i + match_len - start] > new_cost) {
                            dp_cost_ptr[i + match_len - start] = new_cost;
                            dp_from_ptr[i + match_len - start] = i;
                            dp_pos_ptr[i + match_len - start] =
                                subblock_ptr[match_len];
                            const auto &src = states[i];
                            auto       &s = states[i + match_len];
                            s = estimators::state{
                                static_cast<uint32_t>(i - subblock_ptr[match_len]),
                                src.dist_cache[0], src.dist_cache[1]};
                        }
                    }
                }

                // literal
                if (i + 1 <= end) {
                    double new_cost =
                        cur_dp_cost_ptr +
                        est.literal_cost(static_cast<uint64_t>(data_ptr[i]));
                    if (dp_cost_ptr[i + 1 - start] > new_cost) {
                        dp_cost_ptr[i + 1 - start] = new_cost;
                        dp_from_ptr[i + 1 - start] = i;
                        const auto &src = states[i];
                        auto       &s = states[i + 1];
                        s = src;
                    }
                }

                config::processed_bytes++;
                i++;
            }

            finished.count_down();
        });
        block_index++;
    }

    finished.wait();
    {
        using T = decltype(prev);
        T{}.swap(prev);
    }

    config::print_message(std::format("Backtracking dp\n"));
    config::total_bytes = bytes_loaded;
    config::processed_bytes = 0;

    std::reverse(blocks.begin(), blocks.end());
    block_index = blocks.size() - 1;

    std::vector<std::vector<token>> t(blocks.size());
    std::latch                      finished_t(blocks.size());
    for (auto [start, end] : blocks) {
        pool.enqueue([&, start, end, block_index]() mutable {
            end++;
            auto i = end;
            while (i > start) {
                if (dp_cost[block_index][i - start] == INF) {
                    throw std::runtime_error(
                        std::format("Optimal encoder has failed at {}", i));
                }

                uint32_t came_from = dp_from[block_index][i - start];
                if (came_from + 1 == i) {
                    // literal
                    t[block_index].push_back(data[came_from]);
                    i = came_from;
                    config::processed_bytes++;
                    continue;
                }
                uint32_t best_match_len = i - came_from;
                uint32_t best_match_pos = dp_pos[block_index][i - start];
                if (best_match_len == 1) {
                    throw std::runtime_error(std::format(
                        "Optimal encoder has failed miserably at {} with "
                        "best_match_len={}",
                        i, best_match_len));
                }

                t[block_index].push_back(
                    match{came_from - best_match_pos, best_match_len});
                config::processed_bytes += i - came_from;
                i = came_from;
            }

            if (i != start) {
                throw std::runtime_error(std::format("Optimal encoder has failed"));
            }
            finished_t.count_down();
        });
        block_index--;
    }

    finished_t.wait();

    for (auto &e : t) {
        std::reverse(e.begin(), e.end());
        for (auto &ee : e) {
            tokens.push_back(ee);
        }
    }

    write_stats_tokens(est);

    if (pass + 1 != config::passes) {
        est.clear();

        std::vector<uint8_t> controls, lengths, literals, ctx;
        estimators::state    s{0, 0, 0};

        for (const auto &t : tokens) {
            if (std::holds_alternative<std::byte>(t)) {
                controls.push_back(0);
                literals.push_back(static_cast<uint8_t>(std::get<std::byte>(t)));
            } else {
                const auto [d, l] = std::get<match>(t);
                lengths.push_back(static_cast<uint8_t>(l));

                if (s.dist_cache[0] == d) {
                    controls.push_back(1);
                } else if (s.dist_cache[1] == d) {
                    controls.push_back(2);
                } else if (s.dist_cache[2] == d) {
                    controls.push_back(3);
                } else {
                    controls.push_back(4);
                    auto info = dist_bins::get(d);
                    ctx.push_back(info.ctx);
                }

                s = estimators::state{static_cast<uint32_t>(d), s.dist_cache[0],
                                      s.dist_cache[1]};
            }
        }

        est.fill(est.controls_table, controls);
        est.fill(est.lengths_table, lengths);
        est.fill(est.literals_table, literals);
        est.fill(est.ctx_table, ctx);
    }

    return tokens;
}

void mpo_encoder::write_stats_hashes(estimators::smart &est) {
    if (!config::stats) {
        return;
    }

    using Map = __gnu_pbds::gp_hash_table<uint32_t, uint32_t>;

    config::print_message("[stats] Calculating hashes\n");

    Map count[config::prefix_lengths.size()];

    constexpr uint32_t NONE = std::numeric_limits<uint32_t>::max();

    for (int len = config::prefix_lengths.size() - 1; len >= 0; len--) {
        const auto prefix_len = config::prefix_lengths[len];
        config::print_message(
            std::format("[stats] Processing prefix_len {}\n", prefix_len));
        config::total_bytes = bytes_loaded;
        config::processed_bytes = 0;

        if (bytes_loaded < prefix_len) {
            continue;
        }

        const auto &pr = prev[len];

        std::vector<uint8_t> visited(bytes_loaded, 0);
        std::mutex           count_mutex;

        uint32_t block_size = std::max(
            uint32_t{1}, static_cast<uint32_t>(bytes_loaded / config::divisions));
        uint32_t blocks = (bytes_loaded + block_size - 1) / block_size;
        uint32_t start = 0;

        worker_pool pool(config::divisions);
        std::latch  finished(blocks);

        while (start < bytes_loaded) {
            uint32_t end = start + block_size - 1;
            if (end >= bytes_loaded) {
                end = bytes_loaded - 1;
            }

            pool.enqueue([&, start, end, prefix_len, len]() mutable {
                Map local_count;

                for (uint32_t i = end;; i--) {
                    if (i < pr.size()) {
                        std::atomic_ref<uint8_t> v_ref(visited[i]);
                        if (v_ref.exchange(1) == 0) {
                            uint32_t chain_len = 1;
                            uint32_t curr = pr[i];

                            while (curr != NONE) {
                                std::atomic_ref<uint8_t> curr_ref(visited[curr]);
                                if (curr_ref.exchange(1) != 0) {
                                    break;
                                }
                                chain_len++;
                                curr = pr[curr];
                            }

                            auto h = static_cast<uint32_t>(
                                hashes::hashn(data.data() + i, prefix_len));
                            local_count[h] += chain_len;
                        }
                    }
                    if (i == start) {
                        break;
                    }
                }

                std::lock_guard<std::mutex> lock(count_mutex);
                for (auto &[h, c] : local_count) {
                    count[len][h] += c;
                }

                config::processed_bytes += (end - start + 1);
                finished.count_down();
            });

            start = end + 1;
        }

        finished.wait();
    }

    Map count_for_len[config::prefix_lengths.size()];
    for (int len = config::prefix_lengths.size() - 1; len >= 0; len--) {
        for (const auto [h, c] : count[len]) {
            count_for_len[len][c]++;
        }
    }

    const std::string file = "stats/hashchains.csv";
    std::filesystem::create_directories("stats");
    std::ofstream out(file);
    if (!out) {
        throw std::runtime_error("Failed to open " + file);
    }

    out << "file_size,mm,prefix_length,hashchain_length,count\n";
    for (int len = config::prefix_lengths.size() - 1; len >= 0; len--) {
        for (const auto [l, c] : count_for_len[len]) {
            out << bytes_loaded << "," << config::max_matches << ","
                << config::prefix_lengths[len] << "," << l << "," << c << "\n";
        }
    }

    out.close();
}

void mpo_encoder::write_stats_tokens(estimators::smart &est) {
    if (!config::stats) {
        return;
    }

    config::print_message("[stats] Calculating tokens\n");

    const std::string file = "stats/tokens.csv";
    std::filesystem::create_directories("stats");
    std::ofstream out(file);
    if (!out) {
        throw std::runtime_error("Failed to open " + file);
    }

    out << "i,token_type,distance,length,estimated_entropy\n";
    size_t   i = 0;
    uint32_t file_pos = 0;
    for (const auto &t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            out << i << ",lit,0,0,0\n";
            file_pos++;
        } else {
            const auto [d, l] = std::get<match>(t);
            out << i << ",match," << d << "," << l << ","
                << est.match_cost(d, l, states[file_pos]) << "\n";
            file_pos += l;
        }
        i++;
    }

    out.close();
}
