#pragma once

#include <functional>
#include <optional>
#include <span>
#include <utility>

#include "circular_buffer.h"
#include "config.h"
#include "prefix_table.h"
#include "token.h"

class encoder {
    config          cfg;
    circular_buffer buffer;
    prefix_table    prefixes;
    size_t          window_size;
    size_t          future_loaded;

public:
    encoder(const config &config);

    std::optional<token> encode(std::istreambuf_iterator<char> input);
};
