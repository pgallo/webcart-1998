#include "webcart/gateway_protocol.hpp"

#include "webcart/xml.hpp"

namespace webcart {

namespace {

int parse_qty(const std::string& text) {
    if (text.empty() || text.size() > 4) throw ShopError(ErrorCode::InvalidQuantity, "bad quantity");
    for (char c : text)
        if (c < '0' || c > '9') throw ShopError(ErrorCode::InvalidQuantity, "bad quantity");
    return std::stoi(text);
}

std::string required(const xml::Element& req, const std::string& key) {
    auto value = req.attr(key);
    if (value.empty()) throw ShopError(ErrorCode::BadRequest, "missing attribute " + key);
    return value;
}

void write_cart(xml::Writer& w, const CartView& view) {
    const Quote& q = view.quote;
    w.open("cart").attr("id", view.id).attr("promo", q.promo_active ? q.promo_code : "");
    for (const auto& l : q.lines) {
        bool unreserved = false;
        for (const auto& s : view.unreserved_skus) unreserved |= s == l.sku;
        w.open("line")
            .attr("sku", l.sku)
            .attr("name", l.name)
            .attr("qty", l.qty)
            .attr("unitCents", l.unit)
            .attr("netCents", l.net)
            .attr("discountCents", l.discount)
            .attr("vatBp", l.vat_bp)
            .attr("reserved", unreserved ? "false" : "true")
            .close();
    }
    for (const auto& b : q.vat_buckets)
        w.open("vat").attr("rateBp", b.rate_bp).attr("baseCents", b.base).attr("vatCents", b.vat).close();
    w.open("totals")
        .attr("items", q.item_count())
        .attr("subtotalCents", q.subtotal)
        .attr("discountCents", q.discount)
        .attr("vatCents", q.vat)
        .attr("totalCents", q.total)
        .close();
    for (const auto& n : q.notes) w.open("note").text(n).close();
    w.close();
}

void write_order(xml::Writer& w, const OrderRecord& o) {
    w.open("order")
        .attr("id", o.order_id)
        .attr("email", o.email)
        .attr("placedAt", o.placed_at)
        .attr("promo", o.promo_code)
        .attr("subtotalCents", o.subtotal)
        .attr("discountCents", o.discount)
        .attr("vatCents", o.vat)
        .attr("totalCents", o.total);
    for (const auto& l : o.lines)
        w.open("line")
            .attr("sku", l.sku)
            .attr("qty", l.qty)
            .attr("unitCents", l.unit)
            .attr("discountCents", l.discount)
            .attr("vatBp", l.vat_bp)
            .close();
    w.close();
}

}  // namespace

std::string catalog_xml(Shop& shop) {
    xml::Writer w;
    w.open("catalog");
    for (const auto& p : shop.catalog().products())
        w.open("product")
            .attr("sku", p.sku)
            .attr("name", p.name)
            .attr("category", p.category)
            .attr("unitCents", p.unit_price)
            .attr("vatBp", p.vat_bp)
            .attr("available", shop.available(p.sku))
            .close();
    return w.str();
}

std::string cart_xml(const CartView& view) {
    xml::Writer w;
    write_cart(w, view);
    return w.str();
}

std::string order_xml(const OrderRecord& order) {
    xml::Writer w;
    write_order(w, order);
    return w.str();
}

std::string GatewayProtocol::handle(const std::string& request_xml) {
    xml::Writer w;
    try {
        const xml::Element req = xml::parse_single_element(request_xml);
        if (req.name != "request") throw ShopError(ErrorCode::BadRequest, "root must be <request>");
        const std::string op = required(req, "op");

        std::string body;
        if (op == "catalog") {
            body = catalog_xml(shop_);
        } else if (op == "createCart") {
            body = cart_xml(shop_.view_cart(shop_.create_cart()));
        } else if (op == "getCart") {
            body = cart_xml(shop_.view_cart(required(req, "cart")));
        } else if (op == "addItem" || op == "setQuantity" || op == "removeItem") {
            const auto cart = required(req, "cart");
            const auto sku = required(req, "sku");
            if (op == "addItem") shop_.add_item(cart, sku, parse_qty(required(req, "qty")));
            else if (op == "setQuantity") shop_.set_quantity(cart, sku, parse_qty(required(req, "qty")));
            else shop_.remove_item(cart, sku);
            body = cart_xml(shop_.view_cart(cart));
        } else if (op == "applyPromotion") {
            const auto cart = required(req, "cart");
            shop_.apply_promotion(cart, req.attr("code"));
            body = cart_xml(shop_.view_cart(cart));
        } else if (op == "placeOrder") {
            body = order_xml(shop_.place_order(required(req, "cart"), req.attr("email")));
        } else if (op == "getOrder") {
            const auto id = required(req, "order");
            const auto order = shop_.find_order(id);
            if (!order) throw ShopError(ErrorCode::UnknownOrder, "no such order " + id);
            body = order_xml(*order);
        } else {
            throw ShopError(ErrorCode::BadRequest, "unknown op " + op);
        }
        return "<response status=\"ok\">" + body + "</response>";
    } catch (const ShopError& e) {
        w.open("response").attr("status", "error").attr("code", code_name(e.code())).text(e.what());
    } catch (const std::exception& e) {
        w.open("response").attr("status", "error").attr("code", "INTERNAL").text(e.what());
    }
    return w.str();
}

}  // namespace webcart
