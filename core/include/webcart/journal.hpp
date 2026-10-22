#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "webcart/money.hpp"

namespace webcart {

struct OrderLine {
    std::string sku;
    int qty = 0;
    Cents unit = 0;
    Cents discount = 0;
    int vat_bp = 0;
};

struct OrderRecord {
    std::string order_id;
    std::string cart_id;
    std::string email;
    std::int64_t placed_at = 0;  // seconds since epoch
    std::string promo_code;
    std::vector<OrderLine> lines;
    Cents subtotal = 0;
    Cents discount = 0;
    Cents vat = 0;
    Cents total = 0;
};

// Append-only order journal. One order is a group of tab-separated lines:
//
//   B  <order> <cart> <placed_at> <email> <promo>
//   L  <order> <sku> <qty> <unit> <discount> <vat_bp>     (one per line item)
//   T  <order> <subtotal> <discount> <vat> <total>
//   C  <order> <fnv1a-64 of the B..T text>
//
// An order counts only once its C line is on disk with a matching checksum,
// so a crash in the middle of a write leaves no half order behind.
class Journal {
public:
    struct Replay {
        std::vector<OrderRecord> committed;
        std::size_t discarded = 0;   // incomplete or corrupt groups dropped
        std::size_t truncated_bytes = 0;
    };

    // Empty path: in-memory only, nothing persisted.
    explicit Journal(std::string path);

    // Reads committed orders and truncates a torn tail so the next append
    // starts on a clean line.
    Replay open();

    // Writes the whole group with one write and flush; throws ShopError(JournalFailure).
    void append(const OrderRecord& order);

    const std::string& path() const { return path_; }

    static std::string encode(const OrderRecord& order);

private:
    std::string path_;
};

std::uint64_t fnv1a64(const std::string& data);

}  // namespace webcart
