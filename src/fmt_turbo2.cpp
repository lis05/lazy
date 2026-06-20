#include "fmt_turbo2.h"

#include "bins.h"
#include "bitstream.h"
#include "config.h"
#include "formats.h"
#include "turborc.h"

namespace formats::turbo2 {

std::istream& operator>>(std::istream& in, header& h) {
    in.read(reinterpret_cast<char*>(&h.orig_bytes), sizeof(h.orig_bytes));
    in.read(reinterpret_cast<char*>(&h.n_control), sizeof(h.n_control));
    in.read(reinterpret_cast<char*>(&h.n_lit), sizeof(h.n_lit));
    in.read(reinterpret_cast<char*>(&h.n_dist), sizeof(h.n_dist));
    in.read(reinterpret_cast<char*>(&h.n_len), sizeof(h.n_len));
    in.read(reinterpret_cast<char*>(&h.bytes_control), sizeof(h.bytes_control));
    in.read(reinterpret_cast<char*>(&h.bytes_lit), sizeof(h.bytes_lit));
    in.read(reinterpret_cast<char*>(&h.bytes_dist), sizeof(h.bytes_dist));
    in.read(reinterpret_cast<char*>(&h.bytes_len), sizeof(h.bytes_len));
    in.read(reinterpret_cast<char*>(&h.bytes_extra_dist),
            sizeof(h.bytes_extra_dist));
    return in;
}

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
    out << formats::TURBO2;
}

static constexpr int PRM0 = 4;
static constexpr int PRM1 = 7;

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

    ::turborc::compress<::turborc::rcmrrssenc, std::byte, PRM0, PRM1>(control,
                                                                      out_control);
    ::turborc::compress<::turborc::rcmrrssenc, std::byte, PRM0, PRM1>(lit, out_lit);
    ::turborc::compress<::turborc::rcmrrssenc, std::byte, PRM0, PRM1>(dist,
                                                                      out_dist);
    ::turborc::compress<::turborc::rcmrrssenc, uint8_t, PRM0, PRM1>(len, out_len);

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

std::pair<uint64_t, std::vector<token>> read_block(std::istream& in) {
    config::print_message("Decoding tokens (may take a while)\n");
    header h;
    if (!(in >> h)) {
        return {};
    }

    std::vector<std::byte> out_control(h.bytes_control);
    std::vector<std::byte> out_lit(h.bytes_lit);
    std::vector<std::byte> out_dist(h.bytes_dist);
    std::vector<std::byte> out_len(h.bytes_len);
    std::vector<std::byte> extra_dist(h.bytes_extra_dist);

    in.read(reinterpret_cast<char*>(out_control.data()), out_control.size());
    in.read(reinterpret_cast<char*>(out_lit.data()), out_lit.size());
    in.read(reinterpret_cast<char*>(out_dist.data()), out_dist.size());
    in.read(reinterpret_cast<char*>(out_len.data()), out_len.size());
    in.read(reinterpret_cast<char*>(extra_dist.data()), extra_dist.size());

    std::vector<std::byte> control(h.n_control);
    std::vector<std::byte> lit(h.n_lit);
    std::vector<std::byte> dist(h.n_dist);
    std::vector<uint8_t>   len(h.n_len);

    ::turborc::decompress<::turborc::rcmrrssdec, std::byte, PRM0, PRM1>(out_control,
                                                                        control);
    ::turborc::decompress<::turborc::rcmrrssdec, std::byte, PRM0, PRM1>(out_lit,
                                                                        lit);
    ::turborc::decompress<::turborc::rcmrrssdec, std::byte, PRM0, PRM1>(out_dist,
                                                                        dist);
    ::turborc::decompress<::turborc::rcmrrssdec, uint8_t, PRM0, PRM1>(out_len, len);

    bit_reader<decltype(extra_dist.begin())> reader{extra_dist.begin()};

    std::vector<token> tokens;
    tokens.reserve(h.n_control);

    size_t lit_idx = 0;
    size_t dist_idx = 0;
    size_t len_idx = 0;

    uint32_t dist_cache[3] = {0, 0, 0};

    for (size_t i = 0; i < h.n_control; ++i) {
        if (control[i] == std::byte{0}) {
            tokens.push_back(lit[lit_idx++]);
        } else {
            uint32_t d;
            if (control[i] == std::byte{1}) {
                d = dist_cache[0];
            } else if (control[i] == std::byte{2}) {
                d = dist_cache[1];
            } else if (control[i] == std::byte{3}) {
                d = dist_cache[2];
            } else {
                uint32_t d_ctx = static_cast<uint32_t>(dist[dist_idx++]);
                auto     dbins = dist_bins::get_from_ctx(d_ctx);
                uint32_t d_val = reader.read<uint32_t>(dbins.extra_bits);
                d = dbins.base + d_val;
            }

            dist_cache[2] = dist_cache[1];
            dist_cache[1] = dist_cache[0];
            dist_cache[0] = d;

            tokens.push_back(match{
                .distance = d, .length = static_cast<uint32_t>(len[len_idx++]) + 1});
        }
    }

    return {h.orig_bytes, tokens};
}
}  // namespace formats::turbo2
