#pragma once

#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "webcart/catalog.hpp"
#include "webcart/money.hpp"

namespace webcart {

enum class PromoKind {
    MultiBuy,           // automatic: buy `buy` units of `sku`, pay for `pay`
    PercentOff,         // code: percent_bp off the merchandise, above min_subtotal
    CategoryAmountOff,  // code: fixed amount off the lines of `category`
};

struct Promotion {
    std::string code;  // upper case; for MultiBuy only an identifier
    PromoKind kind = PromoKind::PercentOff;
    std::string label;
    std::string sku;
    std::string category;
    int buy = 0;
    int pay = 0;
    int percent_bp = 0;
    Cents amount = 0;
    Cents min_subtotal = 0;

    bool automatic() const { return kind == PromoKind::MultiBuy; }
};

std::vector<Promotion> parse_promotions_csv(std::istream& in);
std::vector<Promotion> load_promotions_csv(const std::string& path);

struct CartLine {
    std::string sku;
    int qty = 0;
};

struct QuoteLine {
    std::string sku;
    std::string name;
    int qty = 0;
    Cents unit = 0;
    Cents net = 0;       // unit * qty
    Cents discount = 0;  // line promotions plus its share of the order discount
    int vat_bp = 0;
    Cents taxable() const { return net - discount; }
};

struct VatBucket {
    int rate_bp = 0;
    Cents base = 0;
    Cents vat = 0;
};

struct Quote {
    std::vector<QuoteLine> lines;
    std::vector<VatBucket> vat_buckets;  // ascending rate
    Cents subtotal = 0;                  // sum of net
    Cents discount = 0;                  // sum of line discounts
    Cents vat = 0;
    Cents total = 0;                     // subtotal - discount + vat
    std::string promo_code;
    bool promo_active = false;
    std::vector<std::string> notes;
    int item_count() const;
};

class PricingEngine {
public:
    PricingEngine(const Catalog& catalog, std::vector<Promotion> promotions);

    const Promotion* find_code(std::string_view code) const;

    // nullopt if the code would take effect on these lines, else the reason it would not.
    std::optional<std::string> why_not_applicable(const Promotion& promo,
                                                  const std::vector<CartLine>& lines) const;

    Quote quote(const std::vector<CartLine>& lines, std::string_view promo_code) const;

private:
    const Catalog& catalog_;
    std::vector<Promotion> promotions_;
};

std::string to_upper(std::string_view s);

}  // namespace webcart
