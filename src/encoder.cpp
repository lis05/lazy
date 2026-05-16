#include "encoder.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>

#include "strmatch.h"

encoder::encoder(const config &config)
    : cfg(config),
      prefixes(),
      buffer(config.window_size),
      future_loaded(0),
      total_loaded(0) {
}

static inline size_t match_str(std::byte *left, std::byte *right,
                               size_t right_limit) {
    return strmatch::match(left, right, right_limit);
}

std::optional<token> encoder::encode(loader ld, bool &end) {
    std::optional<token> res;
    end = false;

    size_t history_size = total_loaded - future_loaded;

    size_t   best_match_len = 0;
    uint32_t best_match_offset = 0;

    for (size_t len = std::min(future_loaded, cfg.prefix_size); len >= 2; len--) {
        auto candidates =
            prefixes.find(std::span<std::byte>{buffer.data() + history_size, len});
        if (candidates != nullptr) {
            for (uint32_t offset : *candidates) {
                size_t match_len =
                    match_str(buffer.data() + history_size - offset,
                              buffer.data() + history_size, future_loaded);
                if (match_len > best_match_len) {
                    best_match_len = match_len;
                    best_match_offset = offset;
                }
            }

            if (best_match_len != 0) {
                break;
            }
        }
    }

    if (best_match_len == 0 && future_loaded != 0) {
        best_match_len = 1;
        res = buffer[history_size];
    } else if (best_match_len > 0) {
        res = match{best_match_offset, (uint32_t)best_match_len};
    }

    if (history_size + best_match_len > cfg.history_size) {
        size_t erase_count = history_size + best_match_len - cfg.history_size;
        for (size_t i = erase_count; i < total_loaded; i++) {
            buffer[i - erase_count] = buffer[i];
        }
        total_loaded -= erase_count;
        history_size = cfg.history_size;
        future_loaded = total_loaded - history_size;
    } else {
        history_size += best_match_len;
        future_loaded -= best_match_len;
    }

    size_t max_load = cfg.window_size - total_loaded;
    size_t loaded = ld(buffer.data() + total_loaded, max_load);
    future_loaded += loaded;
    total_loaded += loaded;

    if (future_loaded == 0) {
        end = true;
        return res;
    }

    prefixes.clear();
    for (size_t len = cfg.prefix_size; len >= 1; len--) {
        for (size_t offset = 1;
             offset <= history_size && len <= offset + future_loaded; offset++) {
            prefixes.insert(
                std::span<std::byte>(buffer.data() + history_size - offset, len),
                offset);
        }
    }

    return res;
}
