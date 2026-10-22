#pragma once

#include <string>

#include "webcart/shop.hpp"

namespace webcart {

// Canonical XML for the three resources. The COM ToXml methods and the
// gateway responses both use these, so every tier sees the same document.
std::string catalog_xml(Shop& shop);
std::string cart_xml(const CartView& view);
std::string order_xml(const OrderRecord& order);

// The HTTP/XML gateway contract shared by the C++ gateway, the ASP gateway
// page and the Java storefront. One request element in, one response element
// out:
//
//   <request op="addItem" cart="C..." sku="TEA-001" qty="2"/>
//   <response status="ok"><cart id="C..." ...>...</cart></response>
//   <response status="error" code="OUT_OF_STOCK">only 1 left of TEA-001</response>
//
// Ops: catalog, createCart, getCart, addItem, setQuantity, removeItem,
// applyPromotion, placeOrder, getOrder.
class GatewayProtocol {
public:
    explicit GatewayProtocol(Shop& shop) : shop_(shop) {}
    std::string handle(const std::string& request_xml);

private:
    Shop& shop_;
};

}  // namespace webcart
