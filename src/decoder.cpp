#include "decoder.h"

#include "cyccpy.h"
#include "fmt.h"

void decoder::decode(size_t orig_size, std::byte* data,
                     const formats::main::streams& s) {
    config::print_message("Restoring original file\n");
    uint32_t data_i = 0;
    uint32_t lit_i = 0;
    uint32_t dist_i = 0;
    auto     sz = s.controls.size();

    auto     controls_ptr = s.controls.data();
    auto     literals_ptr = s.literals.data();
    auto     lengths_ptr = s.lengths.data();
    auto     distances_ptr = s.distances.data();
    uint32_t distance, length;

    for (uint32_t i = 0; i < sz; i++) {
        switch (static_cast<uint8_t>(controls_ptr[i])) {
        case 0:
            data[data_i++] = literals_ptr[lit_i++];
            break;
        default:
            distance = distances_ptr[dist_i];
            length = static_cast<uint32_t>(lengths_ptr[dist_i++]) + 1;
#ifdef LZMPODEBUG
            if (distance > data_i) {
                throw std::runtime_error(
                    std::format("Invalid distance {} > {}", distance, data_i));
            }
#endif
            cyccpy::cyccpy(reinterpret_cast<uint8_t*>(data + data_i - distance),
                           distance, length);
            data_i += length;
        }
    }
}
