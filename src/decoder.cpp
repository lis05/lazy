#include "decoder.h"

#include "cyccpy.h"
#include "fmt.h"

void decoder::decode(size_t orig_size, std::byte* data,
                     const formats::main::streams& s) {
    config::start_action("Restoring original file...");
    auto sz = s.n_controls;

    const auto data_base = data;
    auto       controls_ptr = s.controls;
    auto       literals_ptr = s.literals;
    auto       lengths_ptr = s.lengths;
    auto       distances_ptr = s.distances;
    uint32_t   distance, length;

    for (uint32_t i = 0; i < sz; i++) {
        if (*controls_ptr == std::byte{0}) {
            *(data++) = *(literals_ptr++);
        } else {
            distance = *(distances_ptr++);
#ifdef LZMPODEBUG
            if (distance > static_cast<uint32_t>(data - data_base)) [[unlikely]] {
                throw std::runtime_error(
                    std::format("Invalid distance {} > {}", distance, data_i));
            }
#endif
            length = static_cast<uint32_t>(*(lengths_ptr++)) + 1;
            cyccpy::cyccpy(reinterpret_cast<uint8_t*>(data - distance), distance,
                           length);
            data += length;
        }
        controls_ptr++;
    }
}
