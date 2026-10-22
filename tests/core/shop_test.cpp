#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <thread>

#include "test_support.hpp"

using namespace webcart;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace {

ErrorCode code_of(const std::function<void()>& f) {
    try {
        f();
    } catch (const ShopError& e) {
        return e.code();
    }
    ADD_FAILURE() << "expected ShopError";
    return ErrorCode::BadRequest;
}

}  // namespace

TEST(Shop, AddItemsReservesStock) {
    auto clock = std::make_shared<ManualClock>();
    auto shop = test::make_shop(clock);
    const auto c = shop->create_cart();
    shop->add_item(c, "POT-020", 2);
    EXPECT_EQ(shop->available("POT-020"), 1);
    shop->add_item(c, "POT-020", 1);
    EXPECT_EQ(shop->available("POT-020"), 0);
    EXPECT_EQ(code_of([&] { shop->add_item(c, "POT-020", 1); }), ErrorCode::OutOfStock);
    EXPECT_EQ(shop->view_cart(c).quote.lines[0].qty, 3);  // failed add leaves the cart as it was
    shop->remove_item(c, "POT-020");
    EXPECT_EQ(shop->available("POT-020"), 3);
}

TEST(Shop, ValidationErrors) {
    auto shop = test::make_shop(std::make_shared<ManualClock>());
    const auto c = shop->create_cart();
    EXPECT_EQ(code_of([&] { shop->add_item(c, "NOPE-1", 1); }), ErrorCode::UnknownSku);
    EXPECT_EQ(code_of([&] { shop->add_item(c, "TEA-001", 0); }), ErrorCode::InvalidQuantity);
    EXPECT_EQ(code_of([&] { shop->set_quantity(c, "TEA-001", 100); }), ErrorCode::InvalidQuantity);
    EXPECT_EQ(code_of([&] { shop->add_item("C-missing", "TEA-001", 1); }), ErrorCode::UnknownCart);
    EXPECT_EQ(code_of([&] { shop->apply_promotion(c, "NOSUCH"); }), ErrorCode::UnknownPromotion);
    EXPECT_EQ(code_of([&] { shop->apply_promotion(c, "WELCOME10"); }), ErrorCode::PromotionNotApplicable);
    EXPECT_EQ(code_of([&] { shop->place_order(c, "x@example.org"); }), ErrorCode::EmptyCart);
    shop->add_item(c, "TEA-001", 1);
    EXPECT_EQ(code_of([&] { shop->place_order(c, "not-an-email"); }), ErrorCode::InvalidEmail);
    EXPECT_EQ(code_of([&] { shop->place_order(c, "a@b.c\tx"); }), ErrorCode::InvalidEmail);
}

TEST(Shop, LapsedHoldsAreReleasedAndRetakenAtCheckout) {
    auto clock = std::make_shared<ManualClock>();
    auto shop = test::make_shop(clock);
    const auto a = shop->create_cart();
    const auto b = shop->create_cart();
    shop->add_item(a, "POT-020", 2);
    EXPECT_EQ(code_of([&] { shop->add_item(b, "POT-020", 2); }), ErrorCode::OutOfStock);

    clock->advance(16min);
    EXPECT_EQ(shop->available("POT-020"), 3);
    EXPECT_EQ(shop->view_cart(a).unreserved_skus.size(), 1u);
    shop->add_item(b, "POT-020", 2);

    EXPECT_EQ(code_of([&] { shop->place_order(a, "a@example.org"); }), ErrorCode::OutOfStock);
    shop->set_quantity(a, "POT-020", 1);
    const auto order = shop->place_order(a, "a@example.org");
    EXPECT_EQ(order.order_id, "ORD-000001");
    EXPECT_EQ(shop->available("POT-020"), 0);
    EXPECT_FALSE(shop->cart_exists(a));
}

TEST(Shop, ActivityExtendsHolds) {
    auto clock = std::make_shared<ManualClock>();
    auto shop = test::make_shop(clock);
    const auto a = shop->create_cart();
    shop->add_item(a, "KIT-500", 2);
    for (int i = 0; i < 4; ++i) {
        clock->advance(10min);
        shop->view_cart(a);
    }
    EXPECT_EQ(shop->available("KIT-500"), 0);
}

TEST(Shop, IdleCartsAreDropped) {
    auto clock = std::make_shared<ManualClock>();
    auto shop = test::make_shop(clock);
    const auto a = shop->create_cart();
    shop->add_item(a, "TEA-001", 1);
    clock->advance(61min);
    EXPECT_FALSE(shop->cart_exists(a));
    EXPECT_EQ(code_of([&] { shop->view_cart(a); }), ErrorCode::UnknownCart);
}

TEST(Shop, OrderTotalsMatchQuote) {
    auto shop = test::make_shop(std::make_shared<ManualClock>());
    const auto c = shop->create_cart();
    shop->add_item(c, "TEA-001", 3);
    shop->add_item(c, "BOOK-100", 1);
    shop->apply_promotion(c, "BOOKS5");
    const auto quote = shop->view_cart(c).quote;
    const auto order = shop->place_order(c, "x@example.org");
    EXPECT_EQ(order.total, quote.total);
    EXPECT_EQ(order.promo_code, "BOOKS5");
    EXPECT_EQ(order.discount, 650 + 500);
    ASSERT_TRUE(shop->find_order(order.order_id).has_value());
}

TEST(Shop, RestartReplaysJournalIntoStockAndOrderNumbers) {
    const auto path = fs::temp_directory_path() / "webcart-shop-restart.journal";
    fs::remove(path);
    auto clock = std::make_shared<ManualClock>();
    {
        auto shop = test::make_shop(clock, path.string());
        const auto c = shop->create_cart();
        shop->add_item(c, "CUP-011", 4);
        shop->place_order(c, "x@example.org");
    }
    auto shop = test::make_shop(clock, path.string());
    EXPECT_EQ(shop->startup_report().orders_replayed, 1u);
    EXPECT_EQ(shop->available("CUP-011"), 2);
    const auto c = shop->create_cart();
    shop->add_item(c, "TEA-001", 1);
    EXPECT_EQ(shop->place_order(c, "x@example.org").order_id, "ORD-000002");
    fs::remove(path);
}

TEST(Shop, JournalFailureLeavesStateUntouched) {
    auto clock = std::make_shared<ManualClock>();
    auto shop = test::make_shop(clock, (fs::temp_directory_path() / "no-such-dir" / "j").string());
    const auto c = shop->create_cart();
    shop->add_item(c, "POT-020", 1);
    EXPECT_EQ(code_of([&] { shop->place_order(c, "x@example.org"); }), ErrorCode::JournalFailure);
    EXPECT_TRUE(shop->cart_exists(c));
    EXPECT_EQ(shop->order_count(), 0u);
    EXPECT_EQ(shop->available("POT-020"), 2);  // still held, not sold
}

TEST(Shop, ConcurrentShoppersNeverOversell) {
    auto shop = test::make_shop(std::make_shared<ManualClock>());
    std::atomic<int> sold{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 5; ++i) {
                const auto c = shop->create_cart();
                try {
                    shop->add_item(c, "BOOK-101", 1);
                    shop->place_order(c, "x@example.org");
                    ++sold;
                } catch (const ShopError& e) {
                    EXPECT_EQ(e.code(), ErrorCode::OutOfStock);
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(sold.load(), 4);
    EXPECT_EQ(shop->available("BOOK-101"), 0);
}

// The Windows script and C++ COM client tests assert these exact figures;
// pinning them here means a pricing change fails on Linux first.
TEST(Shop, FiguresUsedByWindowsTests) {
    auto shop = test::make_shop(std::make_shared<ManualClock>());

    const auto smoke = shop->create_cart();  // tests/com_smoke.vbs
    shop->add_item(smoke, "TEA-001", 3);
    shop->add_item(smoke, "BOOK-100", 1);
    shop->apply_promotion(smoke, "books5");
    auto q = shop->view_cart(smoke).quote;
    EXPECT_EQ(q.subtotal, 3750);
    EXPECT_EQ(q.discount, 1150);
    EXPECT_EQ(q.vat, 182);
    EXPECT_EQ(q.total, 2782);

    const auto gateway = shop->create_cart();  // gateway section of com_smoke.vbs
    shop->add_item(gateway, "TEA-002", 2);
    EXPECT_EQ(shop->view_cart(gateway).quote.total, 1584);

    const auto client = shop->create_cart();  // tests/com/com_client_test.cpp
    shop->add_item(client, "TEA-002", 3);
    shop->add_item(client, "CUP-010", 1);
    shop->apply_promotion(client, "WELCOME10");
    q = shop->view_cart(client).quote;
    EXPECT_EQ(q.subtotal, 3450);
    EXPECT_EQ(q.discount, 345);
    EXPECT_EQ(q.vat, 194 + 255);
    EXPECT_EQ(q.total, 3554);
}
