#pragma once

#include <optional>
#include <span>
#include <utility>

#include "circular_buffer.h"
#include "config.h"
#include "token.h"
#include "prefix_table.h"

class encoder {
    config          cfg;
    circular_buffer buffer;
    prefix_table    prefixes;
    size_t          bytes_loaded;

public:
    encoder(const config &config);

    /* Returns the token and how many bytes need to be loaded. Optional is empty if
     * no token could be produced. */
    std::optional<std::pair<token, size_t>> encode();

    /* Loads bytes into the vector. Throws an expection if too many bytes. */
    void load(std::span<std::byte> data);
};
