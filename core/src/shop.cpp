#include "webcart/shop.hpp"

#include <algorithm>
#include <cstdio>

namespace webcart {

bool is_valid_email(const std::string& email) {
    if (email.size() < 3 || email.size() > 254) return false;
    const auto at = email.find('@');
    if (at == std::string::npos || at == 0 || email.find('@', at + 1) != std::string::npos)
        return false;
    const std::string domain = email.substr(at + 1);
    const auto dot = domain.find('.');
    if (dot == std::string::npos || dot == 0 || domain.back() == '.') return false;
    for (unsigned char c : email)
        if (c <= ' ' || c == 0x7f || c == '<' || c == '>' || c == '"' || c == '&' || c == ',')
            return false;
    return true;
}

Shop::Shop(Catalog catalog, std::vector<Promotion> promotions, ShopConfig config,
           std::shared_ptr<Clock> clock)
    : catalog_(std::move(catalog)),
      pricing_(catalog_, std::move(promotions)),
      config_(std::move(config)),
      clock_(std::move(clock)),
      journal_(config_.journal_path) {
    rng_.seed(config_.id_seed != 0 ? config_.id_seed : std::random_device{}());
    for (const auto& p : catalog_.products()) stock_.set_on_hand(p.sku, p.initial_stock);

    // Stock on hand is the catalog snapshot minus every committed order.
    const auto replay = journal_.open();
    for (const auto& order : replay.committed) {
        for (const auto& line : order.lines) stock_.consume(line.sku, line.qty);
        orders_[order.order_id] = order;
        unsigned long long n = 0;
        if (std::sscanf(order.order_id.c_str(), "ORD-%llu", &n) == 1 && n >= next_order_)
            next_order_ = n + 1;
    }
    startup_ = {replay.committed.size(), replay.discarded, replay.truncated_bytes};
}

int Shop::available(const std::string& sku) {
    std::lock_guard lock(mutex_);
    sweep_locked(clock_->now());
    product_or_throw(sku);
    return stock_.available(sku);
}

std::string Shop::create_cart() {
    std::lock_guard lock(mutex_);
    const auto now = clock_->now();
    sweep_locked(now);
    std::string id;
    do {
        id = next_cart_id();
    } while (carts_.count(id) != 0);
    carts_[id].last_used = now;
    return id;
}

bool Shop::cart_exists(const std::string& cart_id) {
    std::lock_guard lock(mutex_);
    sweep_locked(clock_->now());
    return carts_.count(cart_id) != 0;
}

void Shop::add_item(const std::string& cart_id, const std::string& sku, int qty) {
    std::lock_guard lock(mutex_);
    const auto now = clock_->now();
    sweep_locked(now);
    Cart& cart = cart_locked(cart_id, now);
    if (qty < 1) throw ShopError(ErrorCode::InvalidQuantity, "quantity must be at least 1");
    int current = 0;
    for (const auto& l : cart.lines)
        if (l.sku == sku) current = l.qty;
    set_line(cart_id, cart, sku, current + qty, now);
}

void Shop::set_quantity(const std::string& cart_id, const std::string& sku, int qty) {
    std::lock_guard lock(mutex_);
    const auto now = clock_->now();
    sweep_locked(now);
    set_line(cart_id, cart_locked(cart_id, now), sku, qty, now);
}

void Shop::remove_item(const std::string& cart_id, const std::string& sku) {
    std::lock_guard lock(mutex_);
    const auto now = clock_->now();
    sweep_locked(now);
    set_line(cart_id, cart_locked(cart_id, now), sku, 0, now);
}

void Shop::apply_promotion(const std::string& cart_id, const std::string& code) {
    std::lock_guard lock(mutex_);
    const auto now = clock_->now();
    sweep_locked(now);
    Cart& cart = cart_locked(cart_id, now);
    if (code.empty()) {
        cart.promo_code.clear();
        return;
    }
    const Promotion* promo = pricing_.find_code(code);
    if (!promo) throw ShopError(ErrorCode::UnknownPromotion, to_upper(code) + " is not a valid code");
    if (const auto reason = pricing_.why_not_applicable(*promo, cart.lines))
        throw ShopError(ErrorCode::PromotionNotApplicable, *reason);
    cart.promo_code = promo->code;
}

CartView Shop::view_cart(const std::string& cart_id) {
    std::lock_guard lock(mutex_);
    const auto now = clock_->now();
    sweep_locked(now);
    Cart& cart = cart_locked(cart_id, now);
    CartView view;
    view.id = cart_id;
    view.quote = pricing_.quote(cart.lines, cart.promo_code);
    for (const auto& l : cart.lines)
        if (stock_.reserved_by(cart_id, l.sku) < l.qty) view.unreserved_skus.push_back(l.sku);
    return view;
}

OrderRecord Shop::place_order(const std::string& cart_id, const std::string& email) {
    std::lock_guard lock(mutex_);
    const auto now = clock_->now();
    sweep_locked(now);
    Cart& cart = cart_locked(cart_id, now);
    if (!is_valid_email(email)) throw ShopError(ErrorCode::InvalidEmail, "invalid e-mail address");
    if (cart.lines.empty()) throw ShopError(ErrorCode::EmptyCart, "the cart is empty");

    // Holds may have lapsed while the shopper was away: take them again, all
    // or nothing from the shopper's point of view.
    const auto expires = now + config_.reservation_ttl;
    for (const auto& l : cart.lines) {
        if (!stock_.reserve(cart_id, l.sku, l.qty, expires)) {
            const int left = stock_.available(l.sku) + stock_.reserved_by(cart_id, l.sku);
            throw ShopError(ErrorCode::OutOfStock,
                            "only " + std::to_string(left) + " left of " + l.sku);
        }
    }

    const Quote q = pricing_.quote(cart.lines, cart.promo_code);
    char id[32];
    std::snprintf(id, sizeof id, "ORD-%06llu", static_cast<unsigned long long>(next_order_));
    OrderRecord order;
    order.order_id = id;
    order.cart_id = cart_id;
    order.email = email;
    order.placed_at = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    order.promo_code = q.promo_active ? q.promo_code : std::string();
    for (const auto& l : q.lines) order.lines.push_back({l.sku, l.qty, l.unit, l.discount, l.vat_bp});
    order.subtotal = q.subtotal;
    order.discount = q.discount;
    order.vat = q.vat;
    order.total = q.total;

    // Write-ahead: the journal is the commit point. If it throws, memory is
    // untouched and the cart keeps its holds, so the shopper can retry.
    journal_.append(order);

    stock_.commit(cart_id);
    carts_.erase(cart_id);
    orders_[order.order_id] = order;
    ++next_order_;
    return order;
}

std::optional<OrderRecord> Shop::find_order(const std::string& order_id) {
    std::lock_guard lock(mutex_);
    const auto it = orders_.find(order_id);
    if (it == orders_.end()) return std::nullopt;
    return it->second;
}

std::size_t Shop::order_count() {
    std::lock_guard lock(mutex_);
    return orders_.size();
}

std::size_t Shop::sweep() {
    std::lock_guard lock(mutex_);
    return sweep_locked(clock_->now());
}

std::size_t Shop::sweep_locked(TimePoint now) {
    std::size_t released = stock_.expire(now).size();
    for (auto it = carts_.begin(); it != carts_.end();) {
        if (it->second.last_used + config_.cart_idle_ttl <= now) {
            stock_.release_cart(it->first);
            it = carts_.erase(it);
            ++released;
        } else {
            ++it;
        }
    }
    return released;
}

Shop::Cart& Shop::cart_locked(const std::string& cart_id, TimePoint now) {
    const auto it = carts_.find(cart_id);
    if (it == carts_.end()) throw ShopError(ErrorCode::UnknownCart, "no such cart " + cart_id);
    it->second.last_used = now;
    stock_.touch(cart_id, now + config_.reservation_ttl);
    return it->second;
}

const Product& Shop::product_or_throw(const std::string& sku) const {
    const Product* p = catalog_.find(sku);
    if (!p) throw ShopError(ErrorCode::UnknownSku, "unknown sku " + sku);
    return *p;
}

void Shop::check_qty(int qty) const {
    if (qty < 0 || qty > config_.max_line_qty)
        throw ShopError(ErrorCode::InvalidQuantity,
                        "quantity must be between 0 and " + std::to_string(config_.max_line_qty));
}

void Shop::set_line(const std::string& cart_id, Cart& cart, const std::string& sku, int qty,
                    TimePoint now) {
    product_or_throw(sku);
    check_qty(qty);
    if (!stock_.reserve(cart_id, sku, qty, now + config_.reservation_ttl)) {
        const int left = stock_.available(sku) + stock_.reserved_by(cart_id, sku);
        throw ShopError(ErrorCode::OutOfStock, "only " + std::to_string(left) + " left of " + sku);
    }
    const auto it = std::find_if(cart.lines.begin(), cart.lines.end(),
                                 [&](const CartLine& l) { return l.sku == sku; });
    if (qty == 0) {
        if (it != cart.lines.end()) cart.lines.erase(it);
    } else if (it != cart.lines.end()) {
        it->qty = qty;
    } else {
        cart.lines.push_back({sku, qty});
    }
}

std::string Shop::next_cart_id() {
    char buf[24];
    std::snprintf(buf, sizeof buf, "C%016llx", static_cast<unsigned long long>(rng_()));
    return buf;
}

}  // namespace webcart
