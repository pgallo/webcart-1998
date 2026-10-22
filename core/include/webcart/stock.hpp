#pragma once

#include <chrono>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace webcart {

using TimePoint = std::chrono::system_clock::time_point;

class Clock {
public:
    virtual ~Clock() = default;
    virtual TimePoint now() const = 0;
};

class SystemClock final : public Clock {
public:
    TimePoint now() const override { return std::chrono::system_clock::now(); }
};

class ManualClock final : public Clock {
public:
    explicit ManualClock(TimePoint start = TimePoint{} + std::chrono::hours(24 * 365 * 56))
        : now_(start) {}
    TimePoint now() const override { return now_; }
    void advance(std::chrono::seconds s) { now_ += s; }

private:
    TimePoint now_;
};

// Soft reservations: a cart holds units for a limited time. Available stock is
// on_hand minus all live reservations; on_hand only drops when an order commits.
class StockLedger {
public:
    struct Expired {
        std::string cart;
        std::string sku;
        int qty;
    };

    void set_on_hand(const std::string& sku, int qty);
    int on_hand(const std::string& sku) const;
    int reserved(const std::string& sku) const;
    int available(const std::string& sku) const { return on_hand(sku) - reserved(sku); }
    int reserved_by(const std::string& cart, const std::string& sku) const;

    // Sets the cart's hold on `sku` to exactly `qty` (0 releases it). Returns
    // false, changing nothing, if the increase is not available.
    bool reserve(const std::string& cart, const std::string& sku, int qty, TimePoint expires);

    // Sliding expiry: any activity on a cart extends all its holds.
    void touch(const std::string& cart, TimePoint expires);
    void release_cart(const std::string& cart);
    std::vector<Expired> expire(TimePoint now);

    // Turns the cart's holds into permanent stock decrements.
    void commit(const std::string& cart);
    // Used when replaying the order journal at start-up.
    void consume(const std::string& sku, int qty);

private:
    struct Hold {
        int qty = 0;
        TimePoint expires;
    };
    using Key = std::pair<std::string, std::string>;  // cart, sku

    void drop(std::map<Key, Hold>::iterator it);

    std::map<std::string, int> on_hand_;
    std::map<std::string, int> reserved_;
    std::map<Key, Hold> holds_;
};

}  // namespace webcart
