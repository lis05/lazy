#include "encoder.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>

encoder::encoder(const config &config)
    : cfg(config),
      buffer(config.window_size),
      prefixes(),
      window_size(0),
      future_loaded(0) {
}

std::optional<token> encoder::encode(std::istreambuf_iterator<char> input) {
}

