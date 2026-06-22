#include "decoder.h"

#include "cyccpy.h"

void decoder::decode(size_t orig_size, std::byte* data,
                     const std::vector<token>& tokens) {
    config::print_message("Restoring original file\n");
    uint32_t data_i = 0;

    for (auto t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            data[data_i++] = std::get<std::byte>(t);
        } else {
            auto [distance, length] = std::get<match>(t);
            if (data_i + length + 32 <= orig_size) [[likely]] {
                cyccpy::cyccpy32(
                    reinterpret_cast<uint8_t*>(data + data_i - distance), distance,
                    length);
                data_i += length;
            } else [[unlikely]] {
                for (size_t i = data_i - distance, len = 0; len < length;
                     len++, i++) {
                    data[data_i++] = data[i];
                }
            }
        }
    }
}
