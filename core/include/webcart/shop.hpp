#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "webcart/catalog.hpp"
#include "webcart/errors.hpp"
#include "webcart/journal.hpp"
#include "webcart/pricing.hpp"
#include "webcart/stock.hpp"

namespace webcart {

struct ShopConfig {
    std::chrono::seconds reservation_ttl{15 * 60};
    std::chrono::seconds cart_idle_ttl{60 * 60};
    std::string journal_path;  // empty: orders kept in memory only
    std::uint64_t id_seed = 0;  // 0: seed from std::random_device
    int max_line_qty = 99;
};

struct CartView {
    std::string id;
    Quote quote;
    // Lines whose stock hold has lapsed; checkout will try to re-reserve them.
    std::vector<std::string> unreserved_skus;
};

struct StartupReport {
    std::size_t orders_replayed = 0;
    std::size_t groups_discarded = 0;
    std::size_t bytes_truncated = 0;
};

// The business tier. Every presentation layer (COM objects, the HTTP/XML
// gateway, the demo) is a thin adapter over this class. All public methods
// are thread-safe: the COM server runs in the multithreaded apartment.
class Shop {
public:
    Shop(Catalog catalog, std::vector<Promotion> promotions, ShopConfig config,
         std::shared_ptr<Clock> clock);

    const Catalog& catalog() const { return catalog_; }
    const StartupReport& startup_report() const { return startup_; }
    int available(const std::string& sku);

    std::string create_cart();
    bool cart_exists(const std::string& cart_id);
    void add_item(const std::string& cart_id, const std::string& sku, int qty);
    void set_quantity(const std::string& cart_id, const std::string& sku, int qty);
    void remove_item(const std::string& cart_id, const std::string& sku);
    // Empty code removes the current one.
    void apply_promotion(const std::string& cart_id, const std::string& code);
    CartView view_cart(const std::string& cart_id);

    OrderRecord place_order(const std::string& cart_id, const std::string& email);
    std::optional<OrderRecord> find_order(const std::string& order_id);
    std::size_t order_count();

    // Releases lapsed reservations and idle carts; also runs on every call.
    std::size_t sweep();

private:
    struct Cart {
        std::vector<CartLine> lines;
        std::string promo_code;
        TimePoint last_used;
    };

    std::size_t sweep_locked(TimePoint now);
    Cart& cart_locked(const std::string& cart_id, TimePoint now);
    const Product& product_or_throw(const std::string& sku) const;
    void check_qty(int qty) const;
    void set_line(const std::string& cart_id, Cart& cart, const std::string& sku, int qty,
                  TimePoint now);
    std::string next_cart_id();

    Catalog catalog_;
    PricingEngine pricing_;
    ShopConfig config_;
    std::shared_ptr<Clock> clock_;
    Journal journal_;
    StartupReport startup_;

    std::mutex mutex_;
    StockLedger stock_;
    std::map<std::string, Cart> carts_;
    std::map<std::string, OrderRecord> orders_;
    std::uint64_t next_order_ = 1;
    std::mt19937_64 rng_;
};

bool is_valid_email(const std::string& email);

}  // namespace webcart
