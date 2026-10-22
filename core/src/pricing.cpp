#include "webcart/pricing.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <stdexcept>

#include "webcart/errors.hpp"

namespace webcart {

std::string to_upper(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return out;
}

int Quote::item_count() const {
    int n = 0;
    for (const auto& l : lines) n += l.qty;
    return n;
}

std::vector<Promotion> parse_promotions_csv(std::istream& in) {
    // code,kind,label,sku,category,buy,pay,percent_bp,amount_cents,min_subtotal_cents
    std::vector<Promotion> out;
    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (line.empty() || line == "\r" || line[0] == '#') continue;
        if (header) {
            header = false;
            continue;
        }
        const auto f = split_csv_line(line);
        if (f.size() != 10)
            throw ShopError(ErrorCode::BadRequest, "promotion row needs 10 fields: " + line);
        Promotion p;
        p.code = to_upper(f[0]);
        if (f[1] == "multibuy") p.kind = PromoKind::MultiBuy;
        else if (f[1] == "percent") p.kind = PromoKind::PercentOff;
        else if (f[1] == "category_amount") p.kind = PromoKind::CategoryAmountOff;
        else throw ShopError(ErrorCode::BadRequest, "unknown promotion kind " + f[1]);
        p.label = f[2];
        p.sku = f[3];
        p.category = f[4];
        auto num = [](const std::string& s) { return s.empty() ? 0LL : std::stoll(s); };
        p.buy = static_cast<int>(num(f[5]));
        p.pay = static_cast<int>(num(f[6]));
        p.percent_bp = static_cast<int>(num(f[7]));
        p.amount = num(f[8]);
        p.min_subtotal = num(f[9]);
        if (p.kind == PromoKind::MultiBuy && !(p.buy > p.pay && p.pay > 0))
            throw ShopError(ErrorCode::BadRequest, "multibuy needs buy > pay > 0: " + p.code);
        out.push_back(std::move(p));
    }
    return out;
}

std::vector<Promotion> load_promotions_csv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open promotions " + path);
    return parse_promotions_csv(in);
}

PricingEngine::PricingEngine(const Catalog& catalog, std::vector<Promotion> promotions)
    : catalog_(catalog), promotions_(std::move(promotions)) {}

const Promotion* PricingEngine::find_code(std::string_view code) const {
    const std::string wanted = to_upper(code);
    for (const auto& p : promotions_)
        if (!p.automatic() && p.code == wanted) return &p;
    return nullptr;
}

std::optional<std::string> PricingEngine::why_not_applicable(
    const Promotion& promo, const std::vector<CartLine>& lines) const {
    if (lines.empty()) return "the cart is empty";
    Cents subtotal = 0;
    bool category_hit = false;
    for (const auto& l : lines) {
        const Product* p = catalog_.find(l.sku);
        if (!p) continue;
        subtotal += p->unit_price * l.qty;
        if (p->category == promo.category) category_hit = true;
    }
    if (subtotal < promo.min_subtotal)
        return promo.code + " needs a subtotal of at least " + format_cents(promo.min_subtotal);
    if (promo.kind == PromoKind::CategoryAmountOff && !category_hit)
        return promo.code + " applies only to " + promo.category + " items";
    return std::nullopt;
}

namespace {

// Splits `amount` across lines in proportion to `weights` so that the shares
// add up exactly (largest remainder method, ties to the earlier line).
std::vector<Cents> allocate(Cents amount, const std::vector<Cents>& weights) {
    std::vector<Cents> shares(weights.size(), 0);
    Cents total_weight = 0;
    for (Cents w : weights) total_weight += w;
    if (amount <= 0 || total_weight <= 0) return shares;

    std::vector<std::pair<Cents, std::size_t>> remainders;
    Cents assigned = 0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        const Cents scaled = amount * weights[i];
        shares[i] = scaled / total_weight;
        assigned += shares[i];
        remainders.emplace_back(scaled % total_weight, i);
    }
    std::stable_sort(remainders.begin(), remainders.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    for (std::size_t k = 0; assigned < amount; ++k, ++assigned)
        ++shares[remainders[k % remainders.size()].second];
    return shares;
}

}  // namespace

Quote PricingEngine::quote(const std::vector<CartLine>& lines, std::string_view promo_code) const {
    Quote q;
    std::vector<std::string> categories;
    for (const auto& l : lines) {
        const Product* p = catalog_.find(l.sku);
        if (!p) throw ShopError(ErrorCode::UnknownSku, "unknown sku " + l.sku);
        QuoteLine ql;
        ql.sku = p->sku;
        ql.name = p->name;
        ql.qty = l.qty;
        ql.unit = p->unit_price;
        ql.net = p->unit_price * l.qty;
        ql.vat_bp = p->vat_bp;
        for (const auto& promo : promotions_) {
            if (!promo.automatic() || promo.sku != p->sku || l.qty < promo.buy) continue;
            const int free_units = (l.qty / promo.buy) * (promo.buy - promo.pay);
            ql.discount += free_units * p->unit_price;
            q.notes.push_back(promo.label);
        }
        q.subtotal += ql.net;
        categories.push_back(p->category);
        q.lines.push_back(std::move(ql));
    }

    if (!promo_code.empty()) {
        q.promo_code = to_upper(promo_code);
        const Promotion* promo = find_code(promo_code);
        const auto reason = promo ? why_not_applicable(*promo, lines)
                                  : std::optional<std::string>(q.promo_code + " is not a valid code");
        if (reason) {
            q.notes.push_back(*reason);
        } else {
            std::vector<Cents> weights;
            Cents base = 0;
            for (std::size_t i = 0; i < q.lines.size(); ++i) {
                const bool eligible = promo->kind == PromoKind::PercentOff ||
                                      categories[i] == promo->category;
                weights.push_back(eligible ? q.lines[i].taxable() : 0);
                base += weights.back();
            }
            const Cents order_discount = promo->kind == PromoKind::PercentOff
                                             ? mul_div_round(base, promo->percent_bp, 10000)
                                             : std::min(promo->amount, base);
            const auto shares = allocate(order_discount, weights);
            for (std::size_t i = 0; i < q.lines.size(); ++i) q.lines[i].discount += shares[i];
            q.promo_active = order_discount > 0;
            q.notes.push_back(promo->label);
        }
    }

    // VAT is computed once per rate on the discounted base, as on an invoice,
    // not per line: per-line rounding drifts by a cent on large carts.
    std::map<int, Cents> bases;
    for (const auto& l : q.lines) {
        q.discount += l.discount;
        bases[l.vat_bp] += l.taxable();
    }
    for (const auto& [rate, base] : bases) {
        const Cents vat = mul_div_round(base, rate, 10000);
        q.vat_buckets.push_back({rate, base, vat});
        q.vat += vat;
    }
    q.total = q.subtotal - q.discount + q.vat;
    return q;
}

}  // namespace webcart
