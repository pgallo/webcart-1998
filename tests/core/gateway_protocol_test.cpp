#include <gtest/gtest.h>

#include "test_support.hpp"
#include "webcart/gateway_protocol.hpp"
#include "webcart/xml.hpp"

using namespace webcart;

namespace {

std::string attr_of(const std::string& xml, const std::string& element, const std::string& key) {
    const auto el = xml.find("<" + element + " ");
    if (el == std::string::npos) return "";
    const auto k = xml.find(" " + key + "=\"", el);
    if (k == std::string::npos) return "";
    const auto start = k + key.size() + 3;
    return xml.substr(start, xml.find('"', start) - start);
}

}  // namespace

TEST(Xml, ParsesSingleElementWithEntities) {
    const auto el = xml::parse_single_element(
        "<?xml version=\"1.0\"?>\n<request op='addItem' note=\"a &amp; b &#x41;&#233;\" />");
    EXPECT_EQ(el.name, "request");
    EXPECT_EQ(el.attr("op"), "addItem");
    EXPECT_EQ(el.attr("note"), "a & b A\xC3\xA9");
}

TEST(Xml, RejectsContentAndTrailingData) {
    EXPECT_THROW(xml::parse_single_element("<request><x/></request>"), ShopError);
    EXPECT_THROW(xml::parse_single_element("<request/><other/>"), ShopError);
    EXPECT_THROW(xml::parse_single_element("<request a=\"1\" a=\"2\"/>"), ShopError);
    EXPECT_THROW(xml::parse_single_element("<request a=\"&bogus;\"/>"), ShopError);
    EXPECT_NO_THROW(xml::parse_single_element("<request op=\"x\">  </request>"));
}

TEST(Xml, WriterEscapesAndSelfCloses) {
    xml::Writer w;
    w.open("a").attr("q", "x\"<y>").open("b").close().open("c").text("1 & 2").close();
    EXPECT_EQ(w.str(), "<a q=\"x&quot;&lt;y&gt;\"><b/><c>1 &amp; 2</c></a>");
}

TEST(GatewayProtocol, FullShoppingFlow) {
    auto shop = test::make_shop(std::make_shared<ManualClock>());
    GatewayProtocol gw(*shop);

    const auto catalog = gw.handle("<request op=\"catalog\"/>");
    EXPECT_NE(catalog.find("sku=\"KIT-500\""), std::string::npos);

    const auto created = gw.handle("<request op=\"createCart\"/>");
    const auto cart = attr_of(created, "cart", "id");
    ASSERT_FALSE(cart.empty());

    auto r = gw.handle("<request op=\"addItem\" cart=\"" + cart + "\" sku=\"TEA-001\" qty=\"3\"/>");
    EXPECT_EQ(attr_of(r, "response", "status"), "ok");
    EXPECT_EQ(attr_of(r, "totals", "discountCents"), "650");

    r = gw.handle("<request op=\"addItem\" cart=\"" + cart + "\" sku=\"BOOK-101\" qty=\"9\"/>");
    EXPECT_EQ(attr_of(r, "response", "code"), "OUT_OF_STOCK");
    EXPECT_NE(r.find("only 4 left of BOOK-101"), std::string::npos);

    r = gw.handle("<request op=\"addItem\" cart=\"" + cart + "\" sku=\"POT-020\" qty=\"1\"/>");
    r = gw.handle("<request op=\"applyPromotion\" cart=\"" + cart + "\" code=\"welcome10\"/>");
    EXPECT_EQ(attr_of(r, "cart", "promo"), "WELCOME10");

    r = gw.handle("<request op=\"placeOrder\" cart=\"" + cart + "\" email=\"x@example.org\"/>");
    const auto order = attr_of(r, "order", "id");
    EXPECT_EQ(order, "ORD-000001");

    r = gw.handle("<request op=\"getOrder\" order=\"ORD-000001\"/>");
    EXPECT_EQ(attr_of(r, "order", "email"), "x@example.org");
    r = gw.handle("<request op=\"getCart\" cart=\"" + cart + "\"/>");
    EXPECT_EQ(attr_of(r, "response", "code"), "UNKNOWN_CART");
}

TEST(GatewayProtocol, MalformedInputGivesBadRequest) {
    auto shop = test::make_shop(std::make_shared<ManualClock>());
    GatewayProtocol gw(*shop);
    EXPECT_EQ(attr_of(gw.handle("not xml"), "response", "code"), "BAD_REQUEST");
    EXPECT_EQ(attr_of(gw.handle("<request op=\"dance\"/>"), "response", "code"), "BAD_REQUEST");
    EXPECT_EQ(attr_of(gw.handle("<request op=\"addItem\" cart=\"x\" sku=\"TEA-001\" qty=\"-1\"/>"),
                      "response", "code"),
              "INVALID_QUANTITY");
}
