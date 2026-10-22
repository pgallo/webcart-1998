#pragma once

#include <istream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "webcart/money.hpp"

namespace webcart {

struct Product {
    std::string sku;
    std::string name;
    std::string category;
    Cents unit_price = 0;  // net of VAT
    int vat_bp = 0;        // VAT rate in basis points (2200 = 22%)
    int initial_stock = 0;
};

class Catalog {
public:
    Catalog() = default;
    explicit Catalog(std::vector<Product> products);

    static Catalog parse_csv(std::istream& in);
    static Catalog load_csv(const std::string& path);

    const Product* find(std::string_view sku) const;
    const std::vector<Product>& products() const { return products_; }

private:
    std::vector<Product> products_;
    std::unordered_map<std::string, std::size_t> index_;
};

// Splits one CSV record; supports double-quoted fields with "" escapes.
std::vector<std::string> split_csv_line(const std::string& line);

// Shared by every loader: rejects SKUs that could break the journal or markup.
bool is_valid_sku(std::string_view sku);

}  // namespace webcart
