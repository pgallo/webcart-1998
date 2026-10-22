#include "webcart/catalog.hpp"

#include <fstream>
#include <stdexcept>

#include "webcart/errors.hpp"

namespace webcart {

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                current += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                current += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(std::move(current));
            current.clear();
        } else if (c != '\r') {
            current += c;
        }
    }
    fields.push_back(std::move(current));
    return fields;
}

bool is_valid_sku(std::string_view sku) {
    if (sku.empty() || sku.size() > 32) return false;
    for (char c : sku) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok) return false;
    }
    return true;
}

Catalog::Catalog(std::vector<Product> products) : products_(std::move(products)) {
    for (std::size_t i = 0; i < products_.size(); ++i) {
        const Product& p = products_[i];
        if (!is_valid_sku(p.sku))
            throw ShopError(ErrorCode::BadRequest, "invalid sku in catalog: " + p.sku);
        if (p.unit_price < 0 || p.vat_bp < 0 || p.initial_stock < 0)
            throw ShopError(ErrorCode::BadRequest, "negative value in catalog row " + p.sku);
        if (!index_.emplace(p.sku, i).second)
            throw ShopError(ErrorCode::BadRequest, "duplicate sku in catalog: " + p.sku);
    }
}

Catalog Catalog::parse_csv(std::istream& in) {
    std::vector<Product> products;
    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (line.empty() || line == "\r" || line[0] == '#') continue;
        if (header) {
            header = false;
            continue;
        }
        const auto f = split_csv_line(line);
        if (f.size() != 6)
            throw ShopError(ErrorCode::BadRequest, "catalog row needs 6 fields: " + line);
        Product p;
        p.sku = f[0];
        p.name = f[1];
        p.category = f[2];
        p.unit_price = std::stoll(f[3]);
        p.vat_bp = std::stoi(f[4]);
        p.initial_stock = std::stoi(f[5]);
        products.push_back(std::move(p));
    }
    return Catalog(std::move(products));
}

Catalog Catalog::load_csv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open catalog " + path);
    return parse_csv(in);
}

const Product* Catalog::find(std::string_view sku) const {
    const auto it = index_.find(std::string(sku));
    return it == index_.end() ? nullptr : &products_[it->second];
}

}  // namespace webcart
