#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <utility>
#include <vector>

#include "config.h"
#include "token.h"
#include "fmt.h"

class decoder {
public:
    void decode(size_t orig_size, std::byte *data, const formats::main::streams &s);
};
