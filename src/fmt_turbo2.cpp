#include "fmt_turbo2.h"

#include "config.h"
#include "formats.h"
#include "turborc.h"

namespace formats::turbo2 {

std::istream& operator>>(std::istream& in, header& h) {
    in.read(reinterpret_cast<char*>(&h.n_control), sizeof(h.n_control));
    in.read(reinterpret_cast<char*>(&h.n_lit), sizeof(h.n_lit));
    in.read(reinterpret_cast<char*>(&h.n_dist), sizeof(h.n_dist));
    in.read(reinterpret_cast<char*>(&h.n_len), sizeof(h.n_len));
    in.read(reinterpret_cast<char*>(&h.bytes_control), sizeof(h.bytes_control));
    in.read(reinterpret_cast<char*>(&h.bytes_lit), sizeof(h.bytes_lit));
    in.read(reinterpret_cast<char*>(&h.bytes_dist), sizeof(h.bytes_dist));
    in.read(reinterpret_cast<char*>(&h.bytes_len), sizeof(h.bytes_len));
    return in;
}

std::ostream& operator<<(std::ostream& out, const header& h) {
    out.write(reinterpret_cast<const char*>(&h.n_control), sizeof(h.n_control));
    out.write(reinterpret_cast<const char*>(&h.n_lit), sizeof(h.n_lit));
    out.write(reinterpret_cast<const char*>(&h.n_dist), sizeof(h.n_dist));
    out.write(reinterpret_cast<const char*>(&h.n_len), sizeof(h.n_len));
    out.write(reinterpret_cast<const char*>(&h.bytes_control),
              sizeof(h.bytes_control));
    out.write(reinterpret_cast<const char*>(&h.bytes_lit), sizeof(h.bytes_lit));
    out.write(reinterpret_cast<const char*>(&h.bytes_dist), sizeof(h.bytes_dist));
    out.write(reinterpret_cast<const char*>(&h.bytes_len), sizeof(h.bytes_len));
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

    if (config::future_limit == 0) {
        throw std::runtime_error("Invalid future limit: 0");
    }
}

void write_format_mark(std::ostream& out) {
    out << formats::TURBO2;
}

void write_block(const std::vector<token>& tokens, std::ostream& out) {
    std::vector<std::byte> control;
    std::vector<std::byte> lit;
    std::vector<uint32_t>  dist;
    std::vector<uint32_t>  len;
    header                 header;

    for (auto& t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            control.push_back(std::byte{0});
            lit.push_back(std::get<std::byte>(t));
        } else {
            auto m = std::get<match>(t);
            control.push_back(std::byte{1});
            dist.push_back(m.distance);
            len.push_back(m.length);
        }
    }

    std::vector<std::byte> out_control;
    std::vector<std::byte> out_lit;
    std::vector<std::byte> out_dist;
    std::vector<std::byte> out_len;

    ::turborc::compress<::turborc::rcmrrsenc>(control, out_control);
    ::turborc::compress<::turborc::rcmrrsenc>(lit, out_lit);
    ::turborc::compress<::turborc::rcsenc32>(dist, out_dist);
    ::turborc::compress<::turborc::rcsenc32>(len, out_len);

    header.n_control = control.size();
    header.n_lit = lit.size();
    header.n_dist = dist.size();
    header.n_len = len.size();
    header.bytes_control = out_control.size();
    header.bytes_lit = out_lit.size();
    header.bytes_dist = out_dist.size();
    header.bytes_len = out_len.size();

    out << header;
    out.write(reinterpret_cast<const char*>(out_control.data()), out_control.size());
    out.write(reinterpret_cast<const char*>(out_lit.data()), out_lit.size());
    out.write(reinterpret_cast<const char*>(out_dist.data()), out_dist.size());
    out.write(reinterpret_cast<const char*>(out_len.data()), out_len.size());
}

std::vector<token> read_block(std::istream& in) {
    header h;
    if (!(in >> h)) {
        return {};
    }

    std::vector<std::byte> out_control(h.bytes_control);
    std::vector<std::byte> out_lit(h.bytes_lit);
    std::vector<std::byte> out_dist(h.bytes_dist);
    std::vector<std::byte> out_len(h.bytes_len);

    in.read(reinterpret_cast<char*>(out_control.data()), out_control.size());
    in.read(reinterpret_cast<char*>(out_lit.data()), out_lit.size());
    in.read(reinterpret_cast<char*>(out_dist.data()), out_dist.size());
    in.read(reinterpret_cast<char*>(out_len.data()), out_len.size());

    std::vector<std::byte> control(h.n_control);
    std::vector<std::byte> lit(h.n_lit);
    std::vector<uint32_t>  dist(h.n_dist);
    std::vector<uint32_t>  len(h.n_len);

    ::turborc::decompress<::turborc::rcmrrsdec>(out_control, control);
    ::turborc::decompress<::turborc::rcmrrsdec>(out_lit, lit);
    ::turborc::decompress<::turborc::rcsdec32>(out_dist, dist);
    ::turborc::decompress<::turborc::rcsdec32>(out_len, len);

    std::vector<token> tokens;
    tokens.reserve(h.n_control);

    size_t lit_idx = 0;
    size_t dist_idx = 0;
    size_t len_idx = 0;

    for (size_t i = 0; i < h.n_control; ++i) {
        if (control[i] == std::byte{0}) {
            tokens.push_back(lit[lit_idx++]);
        } else {
            tokens.push_back(
                match{.distance = dist[dist_idx++], .length = len[len_idx++]});
        }
    }

    return tokens;
}

}  // namespace formats::turbo2
