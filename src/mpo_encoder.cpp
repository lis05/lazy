#include "mpo_encoder.h"

#include <latch>

#include "worker_pool.h"

mpo_encoder::mpo_encoder()
    : bytes_loaded(0),
      data(config::block_size),
      tokens(),
      head(1),
      prev(),
      dp_cost(),
      dp_from() {
}

std::pair<std::byte *, size_t &> mpo_encoder::for_loading() {
    return {data.data(), bytes_loaded};
}

void mpo_encoder::process(auto future_limit, const auto NONE, auto i,
                          auto &best_match_len, auto &best_match_pos) {
    best_match_len = 1;
    best_match_pos = NONE;

    const auto &data_buf = data.data();

    for (int len = config::prefix_lengths.size() - 1; len >= 0; len--) {
        const auto prefix_len = config::prefix_lengths[len];
        if (prefix_len > future_limit || i + prefix_len >= prev[len].size()) {
            continue;
        }

        const auto &prev_buf = prev[len].data();

        uint64_t count = config::max_matches;
        for (uint32_t pos = prev_buf[i];
             pos != NONE && pos + config::window_size >= i && --count > 0;
             pos = prev_buf[pos]) {
            uint32_t match_len = strmatch::match_simd_loop(
                future_limit, data_buf + pos, data_buf + i);
            if (match_len >= prefix_len && match_len > best_match_len) {
                best_match_len = match_len;
                best_match_pos = pos;
            }
        }

        if (best_match_len >= prefix_len) {
            break;
        }
    }
}

std::vector<token> mpo_encoder::encode() {
    constexpr auto INF = std::numeric_limits<uint64_t>::max();
    constexpr auto NONE = std::numeric_limits<uint32_t>::max();

    size_t bits =
        std::min(size_t{30}, 8 * sizeof(size_t) - std::countl_zero(bytes_loaded));

    prev.resize(config::prefix_lengths.size());
    head = table{bits};

    for (int len = config::prefix_lengths.size() - 1; len >= 0; len--) {
        const auto prefix_len = config::prefix_lengths[len];
        config::print_message(std::format("Processing prefix_len {}\n", prefix_len));

        if (bytes_loaded < prefix_len) {
            throw std::runtime_error("Cannot compress");
        }

        config::total_bytes = bytes_loaded + 1;
        config::processed_bytes = 0;

        auto &pr = prev[len];
        pr.reserve(bytes_loaded - prefix_len + 1);
        for (size_t i = 0; i + prefix_len < bytes_loaded + 1; i++) {
            auto h = hashes::hashn(data.data() + i, prefix_len);
            if (!head.has(h)) {
                pr.push_back(NONE);
            } else {
                pr.push_back(head.get(h));
            }
            head.insert(h, i);
            config::processed_bytes++;
        }
        head.clear();
    }
    head.destroy();

    worker_pool pool(config::divisions);
    std::latch  finished(config::divisions);

    uint32_t block_size = std::max(
        uint32_t{1}, static_cast<uint32_t>(bytes_loaded / config::divisions));
    uint32_t start = 0;

    config::print_message(std::format("Calculating tokens\n"));
    config::total_bytes = bytes_loaded + 1;
    config::processed_bytes = 0;

    dp_best_match_len.resize(bytes_loaded, 1);
    dp_best_match_pos.resize(bytes_loaded, NONE);
    for (size_t block = 0; block < config::divisions; block++) {
        uint32_t end = start + block_size - 1;
        if (block + 1 == config::divisions) {
            end = bytes_loaded - 1;
        }
        if (end >= bytes_loaded) {
            end = bytes_loaded - 1;
        }

        pool.enqueue([&, start, end]() mutable {
            while (start < end) {
                auto future_limit =
                    std::min(bytes_loaded - start, config::future_limit);
                uint32_t best_match_len, best_match_pos;
                process(future_limit, NONE, start, best_match_len, best_match_pos);

                dp_best_match_len[start] = best_match_len;
                dp_best_match_pos[start] = best_match_pos;

                start++;
                config::processed_bytes++;
            }
            finished.count_down();
        });

        start += block_size;
    }

    finished.wait();

    using prevT = decltype(prev);
    prevT().swap(prev);

    config::print_message(std::format("Calculating dp\n"));
    config::total_bytes = bytes_loaded;
    config::processed_bytes = 0;

    dp_cost.resize(bytes_loaded + 1, INF);
    dp_from.resize(bytes_loaded + 1, NONE);
    dp_cost[0] = 0;
    dp_from[0] = 0;
    for (uint32_t i = 0; i < bytes_loaded; i++) {
        auto cur_dp_cost = dp_cost[i];

        uint32_t best_match_len = dp_best_match_len[i];
        uint32_t best_match_pos = dp_best_match_pos[i];

        uint64_t edge_cost = best_match_len == 1
                                 ? INF
                                 : estimate_cost(i - best_match_pos, best_match_len);
        // edge
        if (best_match_len != 1 && edge_cost < 1ll * literal_cost * best_match_len) {
            // float new_cost = cur_dp_cost + edge_cost * COST_MULT;
            uint64_t new_cost = cur_dp_cost + edge_cost;
            if (dp_cost[i + best_match_len] > new_cost) {
                dp_cost[i + best_match_len] = new_cost;
                dp_from[i + best_match_len] = i;
            }
        }

        // literal
        if (i + 1 <= bytes_loaded) {
            // float new_cost = cur_dp_cost + literal_cost * COST_MULT;
            uint64_t new_cost = cur_dp_cost + literal_cost;
            if (dp_cost[i + 1] > new_cost) {
                dp_cost[i + 1] = new_cost;
                dp_from[i + 1] = i;
            }
        }
        config::processed_bytes++;
    }

    config::print_message(std::format("Backtracking dp\n"));
    config::total_bytes = bytes_loaded;
    config::processed_bytes = 0;

    uint32_t i = bytes_loaded;
    while (i > 0) {
        if (dp_cost[i] == INF) {
            throw std::runtime_error(
                std::format("Optimal encoder has failed at {}", i));
        }

        uint32_t came_from = dp_from[i];
        if (came_from + 1 == i) {
            // literal
            tokens.push_back(data[came_from]);
            i = came_from;
            config::processed_bytes++;
            continue;
        }
        // edge
        uint32_t best_match_len = dp_best_match_len[came_from];
        uint32_t best_match_pos = dp_best_match_pos[came_from];
        if (best_match_len == 1) {
            throw std::runtime_error(
                std::format("Optimal encoder has failed miserably at {} with "
                            "best_match_len={}",
                            i, best_match_len));
        }

        tokens.push_back(match{came_from - best_match_pos, best_match_len});
        config::processed_bytes += i - came_from;
        i = came_from;
    }

    std::reverse(tokens.begin(), tokens.end());
    return tokens;
}
