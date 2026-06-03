#include "fmt_readable.h"

#include "config.h"

namespace formats::readable {

std::ostream& operator<<(std::ostream& out, const header& h) {
    return out << h.n_items << " ";
}

std::istream& operator>>(std::istream& in, header& h) {
    return in >> h.n_items;
}

void write_format_mark(std::ostream& out) {
    out << READABLE;
}

void write_block(const std::vector<token>& tokens, std::ostream& out) {
    out << header{static_cast<uint32_t>(tokens.size())};

    for (auto& t : tokens) {
        if (std::holds_alternative<std::byte>(t)) {
            out << "B ";
            out << static_cast<int>(std::get<std::byte>(t)) << "\n";
        } else {
            auto m = std::get<match>(t);
            out << "M " << m.distance << " " << m.length << "\n";
        }
    }
}

std::vector<token> read_block(std::istream& in) {
    std::vector<token> res;
    header             h;
    if (!(in >> h)) {
        return {};
    }

    for (uint32_t i = 0; i < h.n_items; i++) {
        char c;
        in >> c;

        if (c == 'B') {
            int b;
            in >> b;
            res.push_back(static_cast<std::byte>(b));
        } else if (c == 'M') {
            match m;
            in >> m.distance >> m.length;
            res.push_back(m);
        }
    }

    return res;
}

void verify_config() {
    if (config::block_size == 0) {
        throw std::runtime_error("Invalid block size: 0");
    }

    if (config::window_size == 0) {
        throw std::runtime_error("Invalid window size: 0");
    }

    if (config::future_limit == 0) {
        throw std::runtime_error("Invalid future limit: 0");
    }
}
};  // namespace formats::readable
