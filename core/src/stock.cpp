#include "webcart/stock.hpp"

namespace webcart {

void StockLedger::set_on_hand(const std::string& sku, int qty) { on_hand_[sku] = qty; }

int StockLedger::on_hand(const std::string& sku) const {
    const auto it = on_hand_.find(sku);
    return it == on_hand_.end() ? 0 : it->second;
}

int StockLedger::reserved(const std::string& sku) const {
    const auto it = reserved_.find(sku);
    return it == reserved_.end() ? 0 : it->second;
}

int StockLedger::reserved_by(const std::string& cart, const std::string& sku) const {
    const auto it = holds_.find({cart, sku});
    return it == holds_.end() ? 0 : it->second.qty;
}

bool StockLedger::reserve(const std::string& cart, const std::string& sku, int qty,
                          TimePoint expires) {
    const Key key{cart, sku};
    auto it = holds_.find(key);
    const int current = it == holds_.end() ? 0 : it->second.qty;
    const int delta = qty - current;
    if (delta > 0 && delta > available(sku)) return false;
    if (qty <= 0) {
        if (it != holds_.end()) drop(it);
        return true;
    }
    reserved_[sku] += delta;
    Hold& hold = holds_[key];
    hold.qty = qty;
    hold.expires = expires;
    return true;
}

void StockLedger::touch(const std::string& cart, TimePoint expires) {
    for (auto it = holds_.lower_bound({cart, std::string()});
         it != holds_.end() && it->first.first == cart; ++it)
        it->second.expires = expires;
}

void StockLedger::release_cart(const std::string& cart) {
    auto it = holds_.lower_bound({cart, std::string()});
    while (it != holds_.end() && it->first.first == cart) drop(it++);
}

std::vector<StockLedger::Expired> StockLedger::expire(TimePoint now) {
    std::vector<Expired> out;
    for (auto it = holds_.begin(); it != holds_.end();) {
        if (it->second.expires <= now) {
            out.push_back({it->first.first, it->first.second, it->second.qty});
            drop(it++);
        } else {
            ++it;
        }
    }
    return out;
}

void StockLedger::commit(const std::string& cart) {
    auto it = holds_.lower_bound({cart, std::string()});
    while (it != holds_.end() && it->first.first == cart) {
        on_hand_[it->first.second] -= it->second.qty;
        drop(it++);
    }
}

void StockLedger::consume(const std::string& sku, int qty) { on_hand_[sku] -= qty; }

void StockLedger::drop(std::map<Key, Hold>::iterator it) {
    reserved_[it->first.second] -= it->second.qty;
    holds_.erase(it);
}

}  // namespace webcart
