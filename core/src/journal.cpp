#include "webcart/journal.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "webcart/errors.hpp"

namespace webcart {

std::uint64_t fnv1a64(const std::string& data) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

namespace {

std::string hex64(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') {
            out.push_back(std::move(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(std::move(cur));
    return out;
}

void put(std::string& out, std::initializer_list<std::string> fields) {
    bool first = true;
    for (const auto& f : fields) {
        if (f.find_first_of("\t\r\n") != std::string::npos)
            throw ShopError(ErrorCode::JournalFailure, "field contains a separator");
        if (!first) out += '\t';
        out += f;
        first = false;
    }
    out += '\n';
}

std::string body_of(const OrderRecord& o) {
    std::string out;
    put(out, {"B", o.order_id, o.cart_id, std::to_string(o.placed_at), o.email, o.promo_code});
    for (const auto& l : o.lines)
        put(out, {"L", o.order_id, l.sku, std::to_string(l.qty), std::to_string(l.unit),
                  std::to_string(l.discount), std::to_string(l.vat_bp)});
    put(out, {"T", o.order_id, std::to_string(o.subtotal), std::to_string(o.discount),
              std::to_string(o.vat), std::to_string(o.total)});
    return out;
}

}  // namespace

Journal::Journal(std::string path) : path_(std::move(path)) {}

std::string Journal::encode(const OrderRecord& order) {
    std::string body = body_of(order);
    std::string out = body;
    put(out, {"C", order.order_id, hex64(fnv1a64(body))});
    return out;
}

Journal::Replay Journal::open() {
    Replay result;
    if (path_.empty()) return result;
    std::ifstream in(path_, std::ios::binary);
    if (!in) return result;  // first start: no journal yet
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string data = buffer.str();
    in.close();

    std::size_t pos = 0;
    std::size_t valid_end = 0;
    bool in_group = false;
    std::string group_text;
    OrderRecord current;

    auto abandon = [&] {
        if (in_group) ++result.discarded;
        in_group = false;
        group_text.clear();
    };

    while (pos < data.size()) {
        const std::size_t nl = data.find('\n', pos);
        if (nl == std::string::npos) break;  // torn final line
        const std::string line = data.substr(pos, nl - pos);
        const std::size_t next = nl + 1;
        const auto f = split_tabs(line);
        try {
            if (f[0] == "B" && f.size() == 6) {
                abandon();
                in_group = true;
                current = OrderRecord{};
                current.order_id = f[1];
                current.cart_id = f[2];
                current.placed_at = std::stoll(f[3]);
                current.email = f[4];
                current.promo_code = f[5];
                group_text = line + '\n';
            } else if (in_group && f[0] == "L" && f.size() == 7 && f[1] == current.order_id) {
                current.lines.push_back(
                    {f[2], std::stoi(f[3]), std::stoll(f[4]), std::stoll(f[5]), std::stoi(f[6])});
                group_text += line + '\n';
            } else if (in_group && f[0] == "T" && f.size() == 6 && f[1] == current.order_id) {
                current.subtotal = std::stoll(f[2]);
                current.discount = std::stoll(f[3]);
                current.vat = std::stoll(f[4]);
                current.total = std::stoll(f[5]);
                group_text += line + '\n';
            } else if (in_group && f[0] == "C" && f.size() == 3 && f[1] == current.order_id &&
                       f[2] == hex64(fnv1a64(group_text))) {
                result.committed.push_back(current);
                in_group = false;
                group_text.clear();
                valid_end = next;
            } else {
                abandon();
            }
        } catch (const std::exception&) {
            abandon();
        }
        pos = next;
    }
    abandon();

    if (valid_end < data.size()) {
        result.truncated_bytes = data.size() - valid_end;
        std::error_code ec;
        std::filesystem::resize_file(path_, valid_end, ec);
        if (ec)
            throw ShopError(ErrorCode::JournalFailure,
                            "cannot truncate torn journal tail: " + ec.message());
    }
    return result;
}

void Journal::append(const OrderRecord& order) {
    if (path_.empty()) return;
    const std::string text = encode(order);
    std::ofstream out(path_, std::ios::binary | std::ios::app);
    if (!out) throw ShopError(ErrorCode::JournalFailure, "cannot open journal " + path_);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.flush();
    if (!out) throw ShopError(ErrorCode::JournalFailure, "write to journal failed");
}

}  // namespace webcart
