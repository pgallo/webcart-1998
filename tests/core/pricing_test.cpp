#include <gtest/gtest.h>

#include <sstream>

#include "test_support.hpp"
#include "webcart/pricing.hpp"

using namespace webcart;

namespace {

Catalog small_catalog() {
    return Catalog({{"A-1", "Alpha", "books", 1000, 400, 10},
                    {"B-1", "Beta", "kitchen", 333, 2200, 10},
                    {"C-1", "Gamma", "kitchen", 333, 2200, 10}});
}

}  // namespace

TEST(Money, RoundsHalfAwayFromZero) {
    EXPECT_EQ(mul_div_round(1, 1, 2), 1);
    EXPECT_EQ(mul_div_round(-1, 1, 2), -1);
    EXPECT_EQ(mul_div_round(1049, 1000, 10000), 105);  // 104.9
    EXPECT_EQ(mul_div_round(1045, 1000, 10000), 105);  // 104.5
    EXPECT_EQ(format_cents(123456), "1234.56");
    EXPECT_EQ(format_cents(-5), "-0.05");
}

TEST(Csv, QuotedFieldsAndEscapes) {
    const auto f = split_csv_line(R"(X-1,"Mug, ""large""",kitchen)");
    ASSERT_EQ(f.size(), 3u);
    EXPECT_EQ(f[1], "Mug, \"large\"");
}

TEST(Catalog, RejectsDuplicatesAndBadSkus) {
    EXPECT_THROW(Catalog({{"A-1", "x", "c", 1, 0, 0}, {"A-1", "y", "c", 1, 0, 0}}), ShopError);
    EXPECT_THROW(Catalog({{"a 1", "x", "c", 1, 0, 0}}), ShopError);
    EXPECT_TRUE(is_valid_sku("TEA-001"));
    EXPECT_FALSE(is_valid_sku("tea-001"));
    EXPECT_FALSE(is_valid_sku("TEA<1"));
}

TEST(Pricing, VatPerRateOnDiscountedBase) {
    const Catalog cat = small_catalog();
    PricingEngine engine(cat, {});
    const Quote q = engine.quote({{"A-1", 2}, {"B-1", 3}}, "");
    EXPECT_EQ(q.subtotal, 2000 + 999);
    ASSERT_EQ(q.vat_buckets.size(), 2u);
    EXPECT_EQ(q.vat_buckets[0].rate_bp, 400);
    EXPECT_EQ(q.vat_buckets[0].vat, 80);
    EXPECT_EQ(q.vat_buckets[1].vat, 220);  // 999 * 22% = 219.78
    EXPECT_EQ(q.total, 2999 + 300);
}

TEST(Pricing, VatRoundedPerRateNotPerLine) {
    // Two lines at 3.33 and 22%: per line 73.26 -> 73 + 73 = 146,
    // per rate 666 * 22% = 146.52 -> 147. The invoice figure is the latter.
    const Catalog cat = small_catalog();
    PricingEngine engine(cat, {});
    const Quote q = engine.quote({{"B-1", 1}, {"C-1", 1}}, "");
    EXPECT_EQ(q.vat, 147);
}

TEST(Pricing, MultiBuyIsAutomatic) {
    const Catalog cat = test::data_catalog();
    PricingEngine engine(cat, test::data_promotions());
    const Quote q = engine.quote({{"TEA-001", 7}}, "");
    // 7 units: two free (two complete groups of three).
    EXPECT_EQ(q.lines[0].discount, 2 * 650);
    EXPECT_EQ(q.discount, 1300);
    EXPECT_EQ(q.total, 7 * 650 - 1300 + mul_div_round(5 * 650, 1000, 10000));
}

TEST(Pricing, PercentCodeAllocatesExactlyAcrossRates) {
    const Catalog cat = test::data_catalog();
    PricingEngine engine(cat, test::data_promotions());
    const Quote q = engine.quote({{"BOOK-100", 1}, {"CUP-010", 1}, {"TEA-002", 1}}, "welcome10");
    ASSERT_TRUE(q.promo_active);
    EXPECT_EQ(q.promo_code, "WELCOME10");
    const Cents base = 1800 + 1290 + 720;
    EXPECT_EQ(q.discount, mul_div_round(base, 1000, 10000));
    Cents sum = 0;
    for (const auto& l : q.lines) sum += l.discount;
    EXPECT_EQ(sum, q.discount);  // shares add up to the cent
    Cents taxable = 0;
    for (const auto& b : q.vat_buckets) taxable += b.base;
    EXPECT_EQ(taxable, base - q.discount);
}

TEST(Pricing, CodeBelowMinimumIsInactiveWithReason) {
    const Catalog cat = test::data_catalog();
    PricingEngine engine(cat, test::data_promotions());
    const Quote q = engine.quote({{"TEA-002", 1}}, "WELCOME10");
    EXPECT_FALSE(q.promo_active);
    EXPECT_EQ(q.discount, 0);
    ASSERT_FALSE(q.notes.empty());
    EXPECT_NE(q.notes.back().find("at least 30.00"), std::string::npos);
}

TEST(Pricing, CategoryAmountOnlyTouchesCategoryAndIsCapped) {
    const Catalog cat = test::data_catalog();
    PricingEngine engine(cat, test::data_promotions());
    const Quote q = engine.quote({{"BOOK-100", 1}, {"CUP-010", 1}}, "BOOKS5");
    ASSERT_TRUE(q.promo_active);
    EXPECT_EQ(q.lines[0].discount, 500);
    EXPECT_EQ(q.lines[1].discount, 0);

    std::istringstream in("code,kind,label,sku,category,buy,pay,percent_bp,amount_cents,min\n"
                          "BIG,category_amount,big,,books,,,,999999,\n");
    PricingEngine big(cat, parse_promotions_csv(in));
    EXPECT_EQ(big.quote({{"BOOK-100", 1}}, "BIG").discount, 1800);
}

TEST(Pricing, UnknownSkuThrows) {
    const Catalog cat = small_catalog();
    PricingEngine engine(cat, {});
    try {
        engine.quote({{"NOPE", 1}}, "");
        FAIL();
    } catch (const ShopError& e) {
        EXPECT_EQ(e.code(), ErrorCode::UnknownSku);
    }
}
