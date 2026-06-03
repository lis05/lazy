#include "encoder.h"

encoder::encoder()
    : bytes_loaded(0), data(config::block_size), tokens(), head(), prev() {
}

std::pair<std::byte *, size_t &> encoder::for_loading() {
    return {data.data(), bytes_loaded};
}

void encoder::process(const auto data_buf, const auto prev_buf, auto future_limit,
                      const auto NONE, auto i, auto &best_match_len,
                      auto &best_match_pos) {
    best_match_len = 1;
    best_match_pos = NONE;

    uint64_t count = config::max_matches;
    for (uint32_t pos = prev[i];
         pos != NONE && pos + config::window_size >= i && --count > 0;
         pos = prev_buf[pos]) {
        uint32_t match_len =
            strmatch::match_simd_loop(future_limit, data_buf + pos, data_buf + i);
        if (match_len > best_match_len) {
            best_match_len = match_len;
            best_match_pos = pos;
        }
    }
}

std::vector<token> encoder::encode() {
    constexpr auto NONE = std::numeric_limits<uint32_t>::max();

    head.resize(config::total_hashes(), NONE);
    prev.resize(bytes_loaded, NONE);

    std::fill(head.begin(), head.end(), NONE);
    std::fill(prev.begin(), prev.end(), NONE);

    auto calculate_hashes = [this]() {
        for (size_t i = 0; i + 2 < bytes_loaded; i++) {
            auto h = hashes::hash3(data.data() + i);
            prev[i] = head[h];
            head[h] = i;
        }
    };
    calculate_hashes();

    auto data_buf = data.data();
    auto prev_buf = prev.data();

    for (uint32_t i = 0; i < bytes_loaded;) {
        auto old_i = i;

        bool     is_best_skip_literals = true;
        uint32_t best_skip_len = 0;
        match    best_skip_match;

        bool  best_big_match_exists = false;
        match best_big_match;

        // until there will be at least one byte after skip
        for (uint32_t skip = 0;
             skip <= config::lazy_matching && i + skip < bytes_loaded; skip++) {
            uint32_t old_total_len =
                best_skip_len + (best_big_match_exists ? best_big_match.length : 0);
            // old skip
            uint32_t old_cost = is_best_skip_literals
                                    ? literal_cost * best_skip_len
                                    : estimate_cost(best_skip_match.distance,
                                                    best_skip_match.length);
            old_cost +=
                best_big_match_exists
                    ? estimate_cost(best_big_match.distance, best_big_match.length)
                    : 0;

            bool     is_skip_literals = true;
            uint32_t skip_len = skip;
            match    skip_match;

            if (skip > 2) {
                uint32_t pos, len;
                process(data_buf, prev_buf, skip, NONE, i, len, pos);

                if (len == skip) {
                    // found a match for skip
                    if (estimate_cost(i - pos, len) < literal_cost * skip) {
                        // we take it
                        is_skip_literals = false;
                        skip_match = match{i - pos, len};
                    }
                }
                // otherwise we keep literals
            }

            uint32_t big_len = 1;
            uint32_t big_pos = NONE;
            match    big_match;

            {
                uint32_t pos, len;
                uint32_t future_limit =
                    std::min(bytes_loaded - i - skip, config::future_limit);
                process(data_buf, prev_buf, future_limit, NONE, i + skip, len, pos);

                if (pos != NONE) {
                    assert(len > 1);
                    // found a match for big
                    big_match = match{i + skip - pos, len};
                    big_len = len;
                    big_pos = pos;
                }
                // otherwise we will stop
            }

            if (big_pos == NONE) {
                // no big match found, no point in going further
                break;
            }

            uint32_t new_total_len = skip + big_match.length;

            // we do not take bad option
            if (new_total_len < old_total_len) {
                continue;
            }

            uint32_t new_cost = 0;
            // include skip
            new_cost += is_skip_literals
                            ? literal_cost * skip
                            : estimate_cost(skip_match.distance, skip_match.length);
            // include big match
            new_cost += estimate_cost(big_match.distance, big_match.length);

            // old_total_len is less than new_total_len so in the worst case we
            // can take more literals in the old option and get the current one,
            // so we include that in old cost
            old_cost += (new_total_len - old_total_len) * literal_cost;

            // MOMENT OF TRUTH.
            if (new_total_len >= old_total_len && new_cost < old_cost) {
                is_best_skip_literals = is_skip_literals;
                best_skip_len = skip;
                best_skip_match = skip_match;
                best_big_match_exists = true;
                best_big_match = big_match;
            }
        }

        // if no big match or it is unoptimal
        if (!best_big_match_exists ||
            estimate_cost(best_big_match.distance, best_big_match.length) >=
                literal_cost * best_big_match.length) {
            // we take 1 literal and continue;
            tokens.push_back(data[i++]);
        } else {
            // take skip
            if (best_skip_len > 0) {
                if (is_best_skip_literals) {
                    for (uint32_t ii = 0; ii < best_skip_len; ii++) {
                        tokens.push_back(data[i++]);
                    }
                } else {
                    tokens.push_back(best_skip_match);
                    i += best_skip_len;
                }
            }

            tokens.push_back(best_big_match);
            i += best_big_match.length;
        }

        config::processed_bytes += i - old_i;
    }
    return tokens;
}
