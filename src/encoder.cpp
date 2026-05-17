#include "encoder.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>

#include "hashes.h"
#include "strmatch.h"

encoder::encoder()
    : bytes_loaded(0),
      data(config::block_size),
      tokens(),
      head(config::total_hashes),
      prev(config::block_size) {
}

std::pair<std::byte *, size_t &> encoder::for_loading() {
    return {data.data(), bytes_loaded};
}

constexpr uint32_t NONE = std::numeric_limits<uint32_t>::max();

void encoder::reset() {
    bytes_loaded = 0;
    tokens.clear();
    std::fill(head.begin(), head.end(), NONE);
    std::fill(prev.begin(), prev.end(), NONE);
}

const std::vector<token> &encoder::encode() {
    auto calculate_hashes = [this]() {
        for (size_t i = 0; i + 2 < bytes_loaded; i++) {
            auto h = hashes::hash3(data.data() + i);
            prev[i] = head[h];
            head[h] = i;
        }
    };

    calculate_hashes();

    for (uint32_t i = 0; i < bytes_loaded;) {
        uint32_t best_match_len = 1;
        uint32_t best_match_pos = NONE;

        uint32_t future_limit = std::min(bytes_loaded - i, config::future_limit);
        auto     matchf = strmatch::get_match(future_limit);

        size_t count = config::max_matches;
        for (uint32_t pos = prev[i]; pos != NONE && pos + config::window_size >= i;
             pos = prev[pos]) {
            uint32_t match_len = matchf(data.data() + pos, data.data() + i);
            if (match_len > 1) {
                if (count-- == 0) {
                    best_match_len = match_len;
                    best_match_pos = pos;
                    break;
                }
            }

            if (match_len > best_match_len) {
                best_match_len = match_len;
                best_match_pos = pos;
            }
        }

        if (best_match_len == 1) {
            tokens.push_back(data[i]);
        } else {
            tokens.push_back(match{i - best_match_pos, best_match_len});
        }
        i += best_match_len;
    }

    return tokens;
}
