// webcart-demo: drives the business tier through one shopper session and a
// simulated crash, printing what each tier would show.

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "webcart/shop.hpp"

using namespace webcart;
using namespace std::chrono_literals;

namespace {

void print_cart(const CartView& v) {
    const Quote& q = v.quote;
    std::printf("  %-10s %-34s %4s %9s %9s %5s\n", "sku", "item", "qty", "net", "disc", "vat%");
    for (const auto& l : q.lines)
        std::printf("  %-10s %-34s %4d %9s %9s %5s\n", l.sku.c_str(), l.name.c_str(), l.qty,
                    format_cents(l.net).c_str(), format_cents(l.discount).c_str(),
                    format_cents(l.vat_bp).c_str());
    for (const auto& b : q.vat_buckets)
        std::printf("  VAT %5s%% on %9s = %8s\n", format_cents(b.rate_bp).c_str(),
                    format_cents(b.base).c_str(), format_cents(b.vat).c_str());
    std::printf("  subtotal %s  discount %s  vat %s  total EUR %s\n", format_cents(q.subtotal).c_str(),
                format_cents(q.discount).c_str(), format_cents(q.vat).c_str(),
                format_cents(q.total).c_str());
    for (const auto& n : q.notes) std::printf("  note: %s\n", n.c_str());
}

void attempt(const char* what, const std::function<void()>& f) {
    try {
        f();
        std::printf("%s: ok\n", what);
    } catch (const ShopError& e) {
        std::printf("%s: %s (%s)\n", what, code_name(e.code()), e.what());
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string data = argc > 1 ? argv[1] : "data";
    const auto journal = std::filesystem::temp_directory_path() / "webcart-demo.journal";
    std::filesystem::remove(journal);

    auto clock = std::make_shared<ManualClock>();
    ShopConfig cfg;
    cfg.journal_path = journal.string();
    cfg.id_seed = 1998;
    auto make_shop = [&] {
        return std::make_unique<Shop>(Catalog::load_csv(data + "/catalog.csv"),
                                      load_promotions_csv(data + "/promotions.csv"), cfg, clock);
    };
    auto shop = make_shop();

    std::puts("== shopper A fills a cart");
    const auto a = shop->create_cart();
    shop->add_item(a, "TEA-001", 3);
    shop->add_item(a, "BOOK-100", 1);
    shop->add_item(a, "POT-020", 2);
    attempt("apply WELCOME10", [&] { shop->apply_promotion(a, "welcome10"); });
    print_cart(shop->view_cart(a));

    std::puts("\n== shopper B competes for the last teapot");
    const auto b = shop->create_cart();
    std::printf("POT-020 available: %d\n", shop->available("POT-020"));
    attempt("B adds 2 x POT-020", [&] { shop->add_item(b, "POT-020", 2); });
    std::puts("(20 minutes pass, A's holds lapse)");
    clock->advance(20min);
    attempt("B adds 2 x POT-020", [&] { shop->add_item(b, "POT-020", 2); });
    attempt("A checks out", [&] { shop->place_order(a, "a.shopper@example.org"); });
    attempt("A takes 1 teapot instead", [&] { shop->set_quantity(a, "POT-020", 1); });
    OrderRecord order;
    attempt("A checks out", [&] { order = shop->place_order(a, "a.shopper@example.org"); });
    std::printf("order %s total EUR %s\n", order.order_id.c_str(), format_cents(order.total).c_str());

    std::puts("\n== crash during the next order write, then restart");
    {
        std::ofstream torn(journal, std::ios::app | std::ios::binary);
        torn << "B\tORD-000002\t" << b << "\t0\tb@example.org\t\nL\tORD-000002\tPOT-0";
    }
    shop = make_shop();
    const auto& r = shop->startup_report();
    std::printf("replayed %zu order(s), discarded %zu incomplete, truncated %zu bytes\n",
                r.orders_replayed, r.groups_discarded, r.bytes_truncated);
    std::printf("POT-020 available after restart: %d\n", shop->available("POT-020"));
    std::printf("ORD-000001 found after restart: %s\n", shop->find_order("ORD-000001") ? "yes" : "no");
    std::filesystem::remove(journal);
    return 0;
}
