#include "mp_encoder.h"

mp_encoder::mp_encoder()
    : bytes_loaded(0), data(config::block_size), tokens(), head(1), prev() {
}

std::pair<std::byte *, size_t &> mp_encoder::for_loading() {
    return {data.data(), bytes_loaded};
}

void mp_encoder::process(auto future_limit, const auto NONE, auto i,
                         auto &best_match_len, auto &best_match_pos) {
    best_match_len = 1;
    best_match_pos = NONE;

    const auto &data_buf = data.data();

    for (int len = config::max_prefix_lengths - 1; len >= 0; len--) {
        const auto prefix_len = sizes[len];
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
            if (match_len > best_match_len) {
                best_match_len = match_len;
                best_match_pos = pos;
            }
        }

        if (best_match_len >= prefix_len) {
            break;
        }
    }
}

std::vector<token> mp_encoder::encode() {
    constexpr auto NONE = std::numeric_limits<uint32_t>::max();

    size_t bits =
        std::min(size_t{30}, 8 * sizeof(size_t) - std::countl_zero(bytes_loaded));
    prev.resize(config::max_prefix_lengths);
    head = table{bits};

    for (int len = config::max_prefix_lengths - 1; len >= 0; len--) {
        const auto prefix_len = sizes[len];
        std::cerr << "Processing prefix_len=" << prefix_len << "\n";

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
        }

        head.clear();
    }
    head.destroy();

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
                process(skip, NONE, i, len, pos);

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
                process(future_limit, NONE, i + skip, len, pos);

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

#if 0
    size_t lits = 0;
    std::map<uint32_t, size_t> len;
    std::map<uint32_t, size_t> dist;
    for (auto &t: tokens) {
        if (std::holds_alternative<match>(t)) {
            auto [d, l] = std::get<match>(t);
            len[l]++;
            dist[d]++;
        }
        else {
            lits++;
        }
    }

    std::cerr << "Lits: " << lits << "\n";
    int total = 1000;
    for (auto [l, cnt]: len) {
        total--;
        if (total < 0) break;
        std::cerr << "  len " << l << " cnt " << cnt << "\n";
    }
    total = 1000;
    for (auto [l, cnt]: dist) {
        total--;
        if (total < 0) break;
        std::cerr << "  dist " << l << " cnt " << cnt << "\n";
    }
#endif
    return tokens;
}
