#pragma once

#include <functional>
#include <optional>
#include <span>
#include <utility>

#include "config.h"
#include "prefix_table.h"
#include "token.h"

class encoder {
    config                 cfg;
    prefix_table           prefixes;
    std::vector<std::byte> buffer;
    size_t                 total_loaded;
    size_t                 future_loaded;

public:
    // function that is called to load more bytes into the buffer.
    using loader = std::function<size_t(std::byte *, size_t)>;

    encoder(const config &config);

    std::optional<token> encode(loader ld, bool &end);
};
