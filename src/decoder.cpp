#include "decoder.h"

decoder::decoder() : data() {
}

void decoder::reset() {
    data.clear();
}

std::pair<const std::byte*, size_t> decoder::get_bytes() const noexcept {
    return {data.data(), data.size()};
}

void decoder::decode(const std::vector<token>& tokens) {
    for (auto t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            data.push_back(std::get<std::byte>(t));
        } else {
            auto [distance, length] = std::get<match>(t);
            if (distance > data.size()) {
                throw std::runtime_error("Invalid distance");
            }
            for (size_t i = data.size() - distance, len = 0; len < length;
                 len++, i++) {
                data.push_back(data[i]);
            }
        }
    }
}
