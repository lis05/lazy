#include "encoder.h"

#include <assert>
#include <stdexcept>

encoder::encoder(const config &config)
    : cfg(config), buffer(config.window_size), prefixes(), bytes_loaded(0) {
}

std::optional<std::pair<token, size_t>> encoder::encode() {
}

void encoder::load(std::span<std::byte> data) {
    if (data.size() > cfg.window_size) [[unlikely]] {
        throw std::runtime_error("Cannot load more bytes than requested.");
    }

    for (const std::byte b : data) {
        buffer[bytes_loaded++] = b;
    }

    prefixes.clear();
    for (size_t i = 0; i < cfg.history_size; i++) {
    }
}
