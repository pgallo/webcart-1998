#include <gtest/gtest.h>

#include "webcart/stock.hpp"

using namespace webcart;
using namespace std::chrono_literals;

TEST(StockLedger, ReservationsReduceAvailabilityNotOnHand) {
    StockLedger s;
    s.set_on_hand("X", 5);
    const TimePoint t{};
    EXPECT_TRUE(s.reserve("c1", "X", 3, t + 10min));
    EXPECT_EQ(s.on_hand("X"), 5);
    EXPECT_EQ(s.available("X"), 2);
    EXPECT_FALSE(s.reserve("c2", "X", 3, t + 10min));
    EXPECT_EQ(s.reserved_by("c2", "X"), 0);
    EXPECT_TRUE(s.reserve("c2", "X", 2, t + 10min));
    EXPECT_EQ(s.available("X"), 0);
}

TEST(StockLedger, ResizingAHoldOnlyNeedsTheDelta) {
    StockLedger s;
    s.set_on_hand("X", 4);
    const TimePoint t{};
    EXPECT_TRUE(s.reserve("c1", "X", 3, t + 1min));
    EXPECT_TRUE(s.reserve("c1", "X", 4, t + 1min));
    EXPECT_FALSE(s.reserve("c1", "X", 5, t + 1min));
    EXPECT_TRUE(s.reserve("c1", "X", 1, t + 1min));
    EXPECT_EQ(s.available("X"), 3);
    EXPECT_TRUE(s.reserve("c1", "X", 0, t + 1min));
    EXPECT_EQ(s.available("X"), 4);
}

TEST(StockLedger, ExpiryAndSlidingTouch) {
    StockLedger s;
    s.set_on_hand("X", 2);
    s.set_on_hand("Y", 2);
    const TimePoint t{};
    s.reserve("c1", "X", 1, t + 10min);
    s.reserve("c2", "Y", 2, t + 10min);
    s.touch("c1", t + 30min);
    const auto expired = s.expire(t + 15min);
    ASSERT_EQ(expired.size(), 1u);
    EXPECT_EQ(expired[0].cart, "c2");
    EXPECT_EQ(s.available("Y"), 2);
    EXPECT_EQ(s.available("X"), 1);
}

TEST(StockLedger, CommitDecrementsOnHand) {
    StockLedger s;
    s.set_on_hand("X", 3);
    s.reserve("c1", "X", 2, TimePoint{} + 1min);
    s.commit("c1");
    EXPECT_EQ(s.on_hand("X"), 1);
    EXPECT_EQ(s.reserved("X"), 0);
    EXPECT_EQ(s.available("X"), 1);
}

TEST(StockLedger, ReleaseCartLeavesOtherCartsAlone) {
    StockLedger s;
    s.set_on_hand("X", 5);
    s.reserve("c1", "X", 1, TimePoint{} + 1min);
    s.reserve("c10", "X", 2, TimePoint{} + 1min);
    s.release_cart("c1");
    EXPECT_EQ(s.reserved("X"), 2);
}
