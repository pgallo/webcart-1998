#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "webcart/errors.hpp"
#include "webcart/journal.hpp"

using namespace webcart;
namespace fs = std::filesystem;

namespace {

class JournalTest : public ::testing::Test {
protected:
    void SetUp() override {
        path_ = fs::temp_directory_path() /
                ("webcart-journal-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove(path_);
    }
    void TearDown() override { fs::remove(path_); }

    static OrderRecord order(const std::string& id) {
        OrderRecord o;
        o.order_id = id;
        o.cart_id = "C1";
        o.email = "x@example.org";
        o.placed_at = 1700000000;
        o.lines = {{"TEA-001", 2, 650, 0, 1000}, {"CUP-010", 1, 1290, 129, 2200}};
        o.subtotal = 2590;
        o.discount = 129;
        o.vat = 384;
        o.total = 2845;
        return o;
    }

    std::string read_all() const {
        std::ifstream in(path_, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    fs::path path_;
};

}  // namespace

TEST_F(JournalTest, RoundTrip) {
    Journal j(path_.string());
    EXPECT_TRUE(j.open().committed.empty());
    j.append(order("ORD-000001"));
    j.append(order("ORD-000002"));
    const auto r = Journal(path_.string()).open();
    ASSERT_EQ(r.committed.size(), 2u);
    EXPECT_EQ(r.committed[1].order_id, "ORD-000002");
    ASSERT_EQ(r.committed[0].lines.size(), 2u);
    EXPECT_EQ(r.committed[0].lines[1].discount, 129);
    EXPECT_EQ(r.committed[0].total, 2845);
    EXPECT_EQ(r.discarded, 0u);
}

TEST_F(JournalTest, TornTailIsDiscardedAndTruncated) {
    Journal j(path_.string());
    j.open();
    j.append(order("ORD-000001"));
    const auto good_size = fs::file_size(path_);
    const std::string second = Journal::encode(order("ORD-000002"));
    {
        std::ofstream out(path_, std::ios::binary | std::ios::app);
        out << second.substr(0, second.size() - 10);  // crash before the C line completes
    }
    Journal reopened(path_.string());
    const auto r = reopened.open();
    EXPECT_EQ(r.committed.size(), 1u);
    EXPECT_EQ(r.discarded, 1u);
    EXPECT_EQ(fs::file_size(path_), good_size);
    reopened.append(order("ORD-000002"));
    EXPECT_EQ(Journal(path_.string()).open().committed.size(), 2u);
}

TEST_F(JournalTest, ChecksumMismatchRejectsGroup) {
    Journal j(path_.string());
    j.open();
    j.append(order("ORD-000001"));
    std::string text = read_all();
    text.replace(text.find("\t2845"), 5, "\t9999");  // tamper with the total
    {
        std::ofstream out(path_, std::ios::binary | std::ios::trunc);
        out << text;
    }
    const auto r = Journal(path_.string()).open();
    EXPECT_TRUE(r.committed.empty());
    EXPECT_EQ(r.discarded, 1u);
}

TEST_F(JournalTest, SeparatorsInFieldsAreRefused) {
    Journal j(path_.string());
    auto o = order("ORD-000001");
    o.email = "a\tb@example.org";
    EXPECT_THROW(j.append(o), ShopError);
}

TEST_F(JournalTest, UnwritableLocationThrowsJournalFailure) {
    Journal j((path_ / "missing-dir" / "orders.journal").string());
    try {
        j.append(order("ORD-000001"));
        FAIL();
    } catch (const ShopError& e) {
        EXPECT_EQ(e.code(), ErrorCode::JournalFailure);
    }
}
