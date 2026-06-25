#include "fmt.h"

#include <stdexcept>
#include <type_traits>

#include "bins.h"
#include "bitstream.h"
#include "config.h"
#include "formats.h"
#include "fse.h"
#include "huf.h"
#include "rans_static.h"
#include "turborc.h"

namespace formats::main {

std::ostream& operator<<(std::ostream& out, const header& h) {
    out.write(reinterpret_cast<const char*>(&h.orig_bytes), sizeof(h.orig_bytes));
    out.write(reinterpret_cast<const char*>(&h.n_control), sizeof(h.n_control));
    out.write(reinterpret_cast<const char*>(&h.n_lit), sizeof(h.n_lit));
    out.write(reinterpret_cast<const char*>(&h.n_dist), sizeof(h.n_dist));
    out.write(reinterpret_cast<const char*>(&h.n_len), sizeof(h.n_len));
    out.write(reinterpret_cast<const char*>(&h.bytes_control),
              sizeof(h.bytes_control));
    out.write(reinterpret_cast<const char*>(&h.bytes_lit), sizeof(h.bytes_lit));
    out.write(reinterpret_cast<const char*>(&h.bytes_dist), sizeof(h.bytes_dist));
    out.write(reinterpret_cast<const char*>(&h.bytes_len), sizeof(h.bytes_len));
    out.write(reinterpret_cast<const char*>(&h.bytes_extra_dist),
              sizeof(h.bytes_extra_dist));
    return out;
}

const std::byte* header::read(header& h, const std::byte* ptr) {
    std::memcpy(&h.orig_bytes, ptr, sizeof(h.orig_bytes));
    ptr += sizeof(h.orig_bytes);
    std::memcpy(&h.n_control, ptr, sizeof(h.n_control));
    ptr += sizeof(h.n_control);
    std::memcpy(&h.n_lit, ptr, sizeof(h.n_lit));
    ptr += sizeof(h.n_lit);
    std::memcpy(&h.n_dist, ptr, sizeof(h.n_dist));
    ptr += sizeof(h.n_dist);
    std::memcpy(&h.n_len, ptr, sizeof(h.n_len));
    ptr += sizeof(h.n_len);
    std::memcpy(&h.bytes_control, ptr, sizeof(h.bytes_control));
    ptr += sizeof(h.bytes_control);
    std::memcpy(&h.bytes_lit, ptr, sizeof(h.bytes_lit));
    ptr += sizeof(h.bytes_lit);
    std::memcpy(&h.bytes_dist, ptr, sizeof(h.bytes_dist));
    ptr += sizeof(h.bytes_dist);
    std::memcpy(&h.bytes_len, ptr, sizeof(h.bytes_len));
    ptr += sizeof(h.bytes_len);
    std::memcpy(&h.bytes_extra_dist, ptr, sizeof(h.bytes_extra_dist));
    ptr += sizeof(h.bytes_extra_dist);
    return ptr;
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

    if (config::window_size > dist_bins::MAX_VAL()) {
        throw std::runtime_error(
            std::format("Invalid window size {}. Must be at most {}",
                        config::window_size, dist_bins::MAX_VAL()));
    }

    if (config::future_limit == 0) {
        throw std::runtime_error("Invalid future limit: 0");
    }

    if (config::future_limit > 256) {
        throw std::runtime_error("Invalid future limit: must be at most 256");
    }
}

void write_format_mark(std::ostream& out) {
    out << formats::MAIN;
}

static constexpr int PRM0 = 4;
static constexpr int PRM1 = 7;

static void compress_vect(const auto& vect, auto& res) {
    using T = typename std::decay_t<decltype(vect)>::value_type;
    std::vector<std::byte> tmp;

    if (config::use_turborc) {
        ::turborc::compress<::turborc::rcmrrssenc, T, PRM0, PRM1>(vect, tmp);
        res.reserve(tmp.size() + 1);
        res.push_back(std::byte{0});
        res.insert(res.end(), tmp.begin(), tmp.end());
    } else if (config::use_turboans) {
        ::turborc::compress<::turborc::anscdf1enc, T>(vect, tmp);
        res.reserve(tmp.size() + 1);
        res.push_back(std::byte{1});
        res.insert(res.end(), tmp.begin(), tmp.end());
    } else if (config::use_fse) {
        ::fse::compress<T>(vect, tmp);
        res.reserve(tmp.size() + 1);
        res.push_back(std::byte{2});
        res.insert(res.end(), tmp.begin(), tmp.end());
    } else if (config::use_huf) {
        ::huf::compress<T>(vect, tmp);
        res.reserve(tmp.size() + 1);
        res.push_back(std::byte{3});
        res.insert(res.end(), tmp.begin(), tmp.end());
    } else if (config::use_memcpy) {
        res.resize(vect.size() * sizeof(vect[0]) + 1);
        res[0] = std::byte{4};
        std::memcpy(res.data() + 1, vect.data(), vect.size() * sizeof(vect[0]));
    } else if (config::use_rans_static0) {
        ::rans_static::compress<T>(vect, tmp, 0);
        res.reserve(tmp.size() + 1);
        res.push_back(std::byte{5});
        res.insert(res.end(), tmp.begin(), tmp.end());
    } else if (config::use_rans_static1) {
        ::rans_static::compress<T>(vect, tmp, 1);
        res.reserve(tmp.size() + 1);
        res.push_back(std::byte{6});
        res.insert(res.end(), tmp.begin(), tmp.end());
    } else {
        throw std::runtime_error("No compression algorithm selected");
    }
}

static void decompress_vect(const std::byte* from, uint32_t bytes, std::byte* to,
                            uint32_t to_size) {
    std::byte flag = from[0];
    from++;

    if (flag == std::byte{0}) {
        ::turborc::decompress<::turborc::rcmrrssdec, PRM0, PRM1>(from, bytes - 1, to,
                                                                 to_size);
    } else if (flag == std::byte{1}) {
        ::turborc::decompress<::turborc::anscdf1dec>(from, bytes - 1, to, to_size);
    } else if (flag == std::byte{2}) {
        ::fse::decompress(from, bytes - 1, to);
    } else if (flag == std::byte{3}) {
        ::huf::decompress(from, bytes - 1, to);
    } else if (flag == std::byte{4}) {
        std::memcpy(to, from, bytes);
    } else if (flag == std::byte{5}) {
        ::rans_static::decompress(from, bytes - 1, to, to_size, 0);
    } else if (flag == std::byte{6}) {
        ::rans_static::decompress(from, bytes - 1, to, to_size, 1);
    } else {
        throw std::runtime_error("Unknown compression flag");
    }
}

void write_block(const std::vector<token>& tokens, std::ostream& out) {
    config::print_message("Encoding tokens (may take a while)\n");
    std::vector<std::byte> control;
    std::vector<std::byte> lit;
    std::vector<std::byte> dist;
    std::vector<uint8_t>   len;
    header                 header;
    header.orig_bytes = 0;

    std::vector<std::byte> extra_dist;
    auto                   writer = bit_writer{std::back_inserter(extra_dist)};

    uint32_t dist_cache[3] = {0, 0, 0};

    for (size_t i = 0; i < tokens.size(); i++) {
        auto& t = tokens[i];
        if (std::holds_alternative<std::byte>(t)) {
            control.push_back(std::byte{0});
            lit.push_back(std::get<std::byte>(t));
            header.orig_bytes++;
        } else {
            const auto m = std::get<match>(t);
            header.orig_bytes += m.length;

            if (m.distance == dist_cache[0]) {
                control.push_back(std::byte{1});
            } else if (m.distance == dist_cache[1]) {
                control.push_back(std::byte{2});
            } else if (m.distance == dist_cache[2]) {
                control.push_back(std::byte{3});
            } else {
                control.push_back(std::byte{4});
                auto dbins = dist_bins::get(m.distance);
                dist.push_back(static_cast<std::byte>(dbins.ctx));
                auto dist_val = m.distance - dbins.base;
                writer.write(dist_val, dbins.extra_bits);
            }
            len.push_back(m.length - 1);

            dist_cache[2] = dist_cache[1];
            dist_cache[1] = dist_cache[0];
            dist_cache[0] = m.distance;
        }
    }

    writer.flush();

    std::vector<std::byte> out_control;
    std::vector<std::byte> out_lit;
    std::vector<std::byte> out_dist;
    std::vector<std::byte> out_len;

    compress_vect(control, out_control);
    compress_vect(lit, out_lit);
    compress_vect(dist, out_dist);
    compress_vect(len, out_len);

    header.n_control = control.size();
    header.n_lit = lit.size();
    header.n_dist = dist.size();
    header.n_len = len.size();
    header.bytes_control = out_control.size();
    header.bytes_lit = out_lit.size();
    header.bytes_dist = out_dist.size();
    header.bytes_len = out_len.size();
    header.bytes_extra_dist = extra_dist.size();

    out << header;
    out.write(reinterpret_cast<const char*>(out_control.data()), out_control.size());
    out.write(reinterpret_cast<const char*>(out_lit.data()), out_lit.size());
    out.write(reinterpret_cast<const char*>(out_dist.data()), out_dist.size());
    out.write(reinterpret_cast<const char*>(out_len.data()), out_len.size());
    out.write(reinterpret_cast<const char*>(extra_dist.data()), extra_dist.size());
}

std::pair<uint64_t, streams> read_block(const std::byte* ptr) {
    config::print_message("Decoding tokens (may take a while)\n");
    header h;
    ptr = header::read(h, ptr);

    const std::byte* control_ptr = ptr;
    const std::byte* lit_ptr = control_ptr + h.bytes_control;
    const std::byte* dist_ptr = lit_ptr + h.bytes_lit;
    const std::byte* len_ptr = dist_ptr + h.bytes_dist;
    const std::byte* extra_ptr = len_ptr + h.bytes_len;

    auto control =
        static_cast<std::byte*>(std::malloc(h.n_control * sizeof(std::byte)));
    auto lit = static_cast<std::byte*>(std::malloc(h.n_lit * sizeof(std::byte)));
    auto dist = static_cast<std::byte*>(std::malloc(h.n_dist * sizeof(std::byte)));
    auto len = static_cast<uint8_t*>(std::malloc(h.n_len * sizeof(uint8_t)));

    streams res;
    res.distances = static_cast<uint32_t*>(std::malloc(h.n_len * sizeof(uint32_t)));

    decompress_vect(control_ptr, h.bytes_control,
                    reinterpret_cast<std::byte*>(control), h.n_control);
    decompress_vect(lit_ptr, h.bytes_lit, reinterpret_cast<std::byte*>(lit),
                    h.n_lit);
    decompress_vect(dist_ptr, h.bytes_dist, reinterpret_cast<std::byte*>(dist),
                    h.n_dist);
    decompress_vect(len_ptr, h.bytes_len, reinterpret_cast<std::byte*>(len),
                    h.n_len);

    bit_reader reader(extra_ptr);

    size_t   dist_i = 0;
    size_t   lit_idx = 0;
    size_t   dist_idx = 0;
    size_t   len_idx = 0;
    uint32_t dist_cache[3] = {0, 0, 0};

    uint32_t        d, d_ctx, d_val;
    dist_bins::info dbins;
    for (size_t i = 0; i < h.n_control; ++i) {
        switch (static_cast<int>(control[i])) {
        case 1:
            d = dist_cache[0];
            dist_cache[2] = dist_cache[1];
            dist_cache[1] = dist_cache[0];
            dist_cache[0] = d;
            res.distances[dist_i++] = d;
            break;
        case 2:
            d = dist_cache[1];
            dist_cache[2] = dist_cache[1];
            dist_cache[1] = dist_cache[0];
            dist_cache[0] = d;
            res.distances[dist_i++] = d;
            break;
        case 3:
            d = dist_cache[2];
            dist_cache[2] = dist_cache[1];
            dist_cache[1] = dist_cache[0];
            dist_cache[0] = d;
            res.distances[dist_i++] = d;
            break;
        default:
            [[likely]] d_ctx = static_cast<uint32_t>(dist[dist_idx++]);
            dbins = dist_bins::precalculated[d_ctx];
            d_val = reader.read(dbins.extra_bits);
            d = dbins.base + d_val;
            dist_cache[2] = dist_cache[1];
            dist_cache[1] = dist_cache[0];
            dist_cache[0] = d;
            res.distances[dist_i++] = d;
            [[fallthrough]];
        case 0:
            break;
        }
    }

    if (config::metrics) {
        uint32_t controls_cnt[5] = {0};
        for (size_t i = 0; i < h.n_control; i++) {
            controls_cnt[static_cast<int>(control[i])]++;
        }

        uint32_t dist_below_32 = 0;
        for (size_t i = 0; i < h.n_dist; i++) {
            dist_below_32 += res.distances[i] < 32;
        }

        auto fmt = [](auto num) {
            if (num < 1000) {
                return std::format("{}", num);
            } else if (num <= 1000000) {
                return std::format("{:.1f}K", 1.0 * num / 1000);
            } else if (num <= 1000000000) {
                return std::format("{:.1f}M", 1.0 * num / 1000000);
            } else {
                return std::format("{:.1f}G", 1.0 * num / 1000000000);
            }
        };

        config::print_message("=============================\n");
        config::print_message(std::format(
            "controls:  {} ({}, {}, {}, {}, {}), {} bytes\n", fmt(h.n_control),
            fmt(controls_cnt[0]), fmt(controls_cnt[1]), fmt(controls_cnt[2]),
            fmt(controls_cnt[3]), fmt(controls_cnt[4]), fmt(h.bytes_control)));
        config::print_message(std::format("literals:  {}, {} bytes\n", fmt(h.n_lit),
                                          fmt(h.bytes_lit)));
        config::print_message(std::format("distances: {}, {} bytes\n", fmt(h.n_dist),
                                          fmt(h.bytes_dist)));
        config::print_message(std::format("lengths:   {}, {} bytes\n", fmt(h.n_dist),
                                          fmt(h.bytes_len)));
        config::print_message(std::format("extra:     {}, {} bytes\n", fmt(h.n_dist),
                                          fmt(h.bytes_extra_dist)));
        config::print_message(std::format("dist < 32: {} ({:.1f}%)\n",
                                          fmt(dist_below_32),
                                          100.0 * dist_below_32 / h.n_dist));
        config::print_message("=============================\n");
    }

    res.n_controls = h.n_control;
    res.controls = control;
    res.literals = lit;
    res.lengths = len;
    return {h.orig_bytes, res};
}
}  // namespace formats::main
