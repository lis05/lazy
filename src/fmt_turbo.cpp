#include "fmt_turbo.h"

#include "bitstream.h"
#include "config.h"
#include "formats.h"
#include "turborc.h"

namespace formats::turbo {

// clang-format off
//
// First 2^ReservedBits bins match values 1:1. Next Slots bins contain 2 values and
// emit 1 extra bit. Next Slots bins contain 4 values and emit 2 extra bits. And so
// on up until the number of bins reachex MaxBins.
//
// clang-format on
template <uint64_t ReservedBits, uint64_t Slots, uint64_t MaxBins = 256>
struct bins_cfg {
    static constexpr uint64_t MAX_RESERVED = ((uint64_t)1 << ReservedBits) - 1;
    static constexpr uint64_t MAX_VAL() {
        uint64_t ctx = MAX_RESERVED + 1;
        uint64_t slot = 0;
        uint64_t value = ctx;
        uint64_t value_range = 2;

        uint64_t last_good = 0;

        while (ctx < MaxBins &&
               value + value_range - 1 < std::numeric_limits<uint32_t>::max()) {
            last_good = value + value_range - 1;
            ctx++;
            slot++;
            value += value_range;
            if (slot == Slots) {
                slot = 0;
                value_range <<= 1;
            }
        }

        return last_good;
    }

    struct info {
        uint64_t ctx;
        uint64_t extra_bits;
        uint64_t base;
    };

    static constexpr uint64_t blog2(uint64_t x) {
        return x == 0 ? 0 : 63 - std::countl_zero(x);
    }

    static inline info get(uint64_t x) {
        if (x <= MAX_RESERVED) {
            return info{x, 0, x};
        }

        uint64_t ctx = MAX_RESERVED + 1;
        uint64_t slot = 0;
        uint64_t value = ctx;
        uint64_t value_range = 2;

        while (ctx < MaxBins && value + value_range - 1 < MAX_VAL()) {
            if (value <= x && x < value + value_range) {
                return info{ctx, blog2(value_range), value};
            }
            ctx++;
            slot++;
            value += value_range;
            if (slot == Slots) {
                slot = 0;
                value_range <<= 1;
            }
        }

        throw std::runtime_error(std::format(
            "Bad number {}. Cannot process: too large ctx/value range. :C", x));
        return {};
    }

    static inline info get_from_ctx(uint64_t c) {
        if (c <= MAX_RESERVED) {
            return info{c, 0, c};
        }

        uint64_t ctx = MAX_RESERVED + 1;
        uint64_t slot = 0;
        uint64_t value = ctx;
        uint64_t value_range = 2;

        while (ctx < MaxBins && value + value_range - 1 < MAX_VAL()) {
            if (ctx == c) {
                return info{ctx, blog2(value_range), value};
            }
            ctx++;
            slot++;
            value += value_range;
            if (slot == Slots) {
                slot = 0;
                value_range <<= 1;
            }
        }

        throw std::runtime_error(std::format(
            "Bad context {}. Cannot process: too large ctx/value range. :C", c));
        return {};
    }
};

using dist_bins = bins_cfg<3, 8>;
using len_bins = bins_cfg<3, 8>;

std::istream& operator>>(std::istream& in, header& h) {
    in.read(reinterpret_cast<char*>(&h.n_dist_ctx), sizeof(h.n_dist_ctx));
    in.read(reinterpret_cast<char*>(&h.n_len_ctx), sizeof(h.n_len_ctx));
    in.read(reinterpret_cast<char*>(&h.n_lit_ctx), sizeof(h.n_lit_ctx));
    in.read(reinterpret_cast<char*>(&h.bytes_dist_ctx), sizeof(h.bytes_dist_ctx));
    in.read(reinterpret_cast<char*>(&h.bytes_len_ctx), sizeof(h.bytes_len_ctx));
    in.read(reinterpret_cast<char*>(&h.bytes_lit_ctx), sizeof(h.bytes_lit_ctx));
    in.read(reinterpret_cast<char*>(&h.bytes_extra), sizeof(h.bytes_extra));
    return in;
}

std::ostream& operator<<(std::ostream& out, const header& h) {
    out.write(reinterpret_cast<const char*>(&h.n_dist_ctx), sizeof(h.n_dist_ctx));
    out.write(reinterpret_cast<const char*>(&h.n_len_ctx), sizeof(h.n_len_ctx));
    out.write(reinterpret_cast<const char*>(&h.n_lit_ctx), sizeof(h.n_lit_ctx));
    out.write(reinterpret_cast<const char*>(&h.bytes_dist_ctx),
              sizeof(h.bytes_dist_ctx));
    out.write(reinterpret_cast<const char*>(&h.bytes_len_ctx),
              sizeof(h.bytes_len_ctx));
    out.write(reinterpret_cast<const char*>(&h.bytes_lit_ctx),
              sizeof(h.bytes_lit_ctx));
    out.write(reinterpret_cast<const char*>(&h.bytes_extra), sizeof(h.bytes_extra));
    return out;
}

void verify_config() {
    if (config::block_size == 0) {
        throw std::runtime_error("Invalid block size: 0");
    }

    if (config::window_size == 0) {
        throw std::runtime_error("Invalid window size: 0");
    }

    if (config::window_size < 64) {
        throw std::runtime_error(std::format(
            "Invalid window size: {}. Must be at least 64", config::window_size));
    }

    if (config::window_size > dist_bins::MAX_VAL() ||
        config::window_size > len_bins::MAX_VAL()) {
        throw std::runtime_error(std::format(
            "Invalid window size {}. Must be at most {}", config::window_size,
            std::min(dist_bins::MAX_VAL(), len_bins::MAX_VAL())));
    }

    if (config::future_limit == 0) {
        throw std::runtime_error("Invalid future limit: 0");
    }

    if (config::future_limit > len_bins::MAX_VAL()) {
        throw std::runtime_error(
            std::format("Invalid future limit: {}. Must be at most {}",
                        config::future_limit, len_bins::MAX_VAL()));
    }
}

void write_format_mark(std::ostream& out) {
    out << formats::TURBO;
}

void write_block(const std::vector<token>& tokens, std::ostream& out) {
    std::vector<std::byte> dist_ctx;
    std::vector<std::byte> len_ctx;
    std::vector<std::byte> literals;
    header                 header;

    for (auto& t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            dist_ctx.push_back(static_cast<std::byte>(0));
            literals.push_back(std::get<std::byte>(t));
        } else {
            auto m = std::get<match>(t);
            dist_ctx.push_back(
                static_cast<std::byte>(dist_bins::get(m.distance).ctx));
            len_ctx.push_back(static_cast<std::byte>(len_bins::get(m.length).ctx));
        }
    }

    header.n_dist_ctx = dist_ctx.size();
    header.n_len_ctx = len_ctx.size();
    header.n_lit_ctx = literals.size();

    std::vector<std::byte> out_dist;
    std::vector<std::byte> out_len;
    std::vector<std::byte> out_lit;

    ::turborc::compress<::turborc::rcmrrssenc, std::byte, 4, 7>(dist_ctx, out_dist);
    ::turborc::compress<::turborc::rcmrrssenc, std::byte, 4, 7>(len_ctx, out_len);
    ::turborc::compress<::turborc::rcmrrssenc, std::byte, 4, 7>(literals, out_lit);
    header.bytes_dist_ctx = out_dist.size();
    header.bytes_len_ctx = out_len.size();
    header.bytes_lit_ctx = out_lit.size();

    std::vector<std::byte> out_extra;
    auto                   writer = bit_writer{std::back_inserter(out_extra)};

    for (auto& t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            continue;
        }

        auto m = std::get<match>(t);
        auto dbins = dist_bins::get(m.distance);
        auto lbins = len_bins::get(m.length);
        auto dist_val = m.distance - dbins.base;
        auto len_val = m.length - lbins.base;

        writer.write(dist_val, dbins.extra_bits);
        writer.write(len_val, lbins.extra_bits);
    }

    writer.flush();
    header.bytes_extra = out_extra.size();

    out << header;
    out.write(reinterpret_cast<const char*>(out_dist.data()), out_dist.size());
    out.write(reinterpret_cast<const char*>(out_len.data()), out_len.size());
    out.write(reinterpret_cast<const char*>(out_lit.data()), out_lit.size());
    out.write(reinterpret_cast<const char*>(out_extra.data()), out_extra.size());
}

std::vector<token> read_block(std::istream& in) {
    header h;
    if (!(in >> h)) {
        return {};
    }

    std::vector<std::byte> out_dist(h.bytes_dist_ctx);
    std::vector<std::byte> out_len(h.bytes_len_ctx);
    std::vector<std::byte> out_lit(h.bytes_lit_ctx);
    in.read(reinterpret_cast<char*>(out_dist.data()), out_dist.size());
    in.read(reinterpret_cast<char*>(out_len.data()), out_len.size());
    in.read(reinterpret_cast<char*>(out_lit.data()), out_lit.size());

    std::vector<std::byte> dist_ctx(h.n_dist_ctx);
    std::vector<std::byte> len_ctx(h.n_len_ctx);
    std::vector<std::byte> literals(h.n_lit_ctx);

    ::turborc::decompress<::turborc::rcmrrssdec, std::byte, 4, 7>(out_dist, dist_ctx);
    ::turborc::decompress<::turborc::rcmrrssdec, std::byte, 4, 7>(out_len, len_ctx);
    ::turborc::decompress<::turborc::rcmrrssdec, std::byte, 4, 7>(out_lit, literals);

    std::vector<std::byte> extra(h.bytes_extra);
    in.read(reinterpret_cast<char*>(extra.data()), h.bytes_extra);

    bit_reader<decltype(extra.begin())> reader{extra.begin()};

    std::vector<token> tokens;
    tokens.reserve(h.n_dist_ctx);

    size_t len_idx = 0;
    size_t lit_idx = 0;
    for (size_t i = 0; i < h.n_dist_ctx; ++i) {
        uint32_t d_ctx = static_cast<uint32_t>(dist_ctx[i]);
        if (d_ctx == 0) {
            tokens.push_back(static_cast<std::byte>(literals[lit_idx++]));
        } else {
            auto     dbins = dist_bins::get_from_ctx(d_ctx);
            uint32_t d_val = reader.read<uint32_t>(dbins.extra_bits);
            uint32_t dist = dbins.base + d_val;

            uint32_t l_ctx = static_cast<uint32_t>(len_ctx[len_idx++]);
            auto     lbins = len_bins::get_from_ctx(l_ctx);
            uint32_t l_val = reader.read<uint32_t>(lbins.extra_bits);
            uint32_t len = lbins.base + l_val;

            tokens.push_back(match{.distance = dist, .length = len});
        }
    }

    return tokens;
}

}  // namespace formats::turbo
