#include "objects.hpp"

namespace webcart::com {

namespace {

const Product& product(BSTR sku) {
    const Product* p = shop().catalog().find(utf8(sku));
    if (!p) throw ShopError(ErrorCode::UnknownSku, "unknown sku " + utf8(sku));
    return *p;
}

void check_index(long index, std::size_t size) {
    if (index < 0 || static_cast<std::size_t>(index) >= size)
        throw ShopError(ErrorCode::BadRequest, "index " + std::to_string(index) + " out of range");
}

long to_long(Cents c) { return static_cast<long>(c); }

}  // namespace

// ---- Catalog --------------------------------------------------------------

STDMETHODIMP CatalogObject::get_Count(long* count) {
    return guard([&] { *out_param(count) = static_cast<long>(shop().catalog().products().size()); });
}

STDMETHODIMP CatalogObject::SkuAt(long index, BSTR* sku) {
    return guard([&] {
        out_param(sku);
        const auto& products = shop().catalog().products();
        check_index(index, products.size());
        *sku = to_bstr(products[static_cast<std::size_t>(index)].sku);
    });
}

STDMETHODIMP CatalogObject::ProductName(BSTR sku, BSTR* name) {
    return guard([&] {
        out_param(name);
        *name = to_bstr(product(sku).name);
    });
}

STDMETHODIMP CatalogObject::Category(BSTR sku, BSTR* category) {
    return guard([&] {
        out_param(category);
        *category = to_bstr(product(sku).category);
    });
}

STDMETHODIMP CatalogObject::UnitPriceCents(BSTR sku, long* cents) {
    return guard([&] { *out_param(cents) = to_long(product(sku).unit_price); });
}

STDMETHODIMP CatalogObject::VatRateBp(BSTR sku, long* basis_points) {
    return guard([&] { *out_param(basis_points) = product(sku).vat_bp; });
}

STDMETHODIMP CatalogObject::Available(BSTR sku, long* units) {
    return guard([&] { *out_param(units) = shop().available(product(sku).sku); });
}

STDMETHODIMP CatalogObject::ToXml(BSTR* xml) {
    return guard([&] {
        out_param(xml);
        *xml = to_bstr(catalog_xml(shop()));
    });
}

// ---- Cart -----------------------------------------------------------------

const std::string& CartObject::bound() const {
    if (cart_id_.empty())
        throw ShopError(ErrorCode::UnknownCart, "cart handle not bound: call Create or Open first");
    return cart_id_;
}

QuoteLine CartObject::line(long index) const {
    const auto view = shop().view_cart(bound());
    check_index(index, view.quote.lines.size());
    return view.quote.lines[static_cast<std::size_t>(index)];
}

STDMETHODIMP CartObject::Create(BSTR* cart_id) {
    return guard([&] {
        out_param(cart_id);
        cart_id_ = shop().create_cart();
        *cart_id = to_bstr(cart_id_);
    });
}

STDMETHODIMP CartObject::Open(BSTR cart_id, VARIANT_BOOL* found) {
    return guard([&] {
        out_param(found);
        const std::string id = utf8(cart_id);
        const bool exists = !id.empty() && shop().cart_exists(id);
        cart_id_ = exists ? id : std::string();
        *found = exists ? VARIANT_TRUE : VARIANT_FALSE;
    });
}

STDMETHODIMP CartObject::get_CartId(BSTR* cart_id) {
    return guard([&] {
        out_param(cart_id);
        *cart_id = to_bstr(cart_id_);
    });
}

STDMETHODIMP CartObject::AddItem(BSTR sku, long quantity) {
    return guard([&] { shop().add_item(bound(), utf8(sku), static_cast<int>(quantity)); });
}

STDMETHODIMP CartObject::SetQuantity(BSTR sku, long quantity) {
    return guard([&] { shop().set_quantity(bound(), utf8(sku), static_cast<int>(quantity)); });
}

STDMETHODIMP CartObject::RemoveItem(BSTR sku) {
    return guard([&] { shop().remove_item(bound(), utf8(sku)); });
}

STDMETHODIMP CartObject::ApplyPromotion(BSTR code) {
    return guard([&] { shop().apply_promotion(bound(), utf8(code)); });
}

STDMETHODIMP CartObject::get_LineCount(long* count) {
    return guard([&] {
        *out_param(count) = static_cast<long>(shop().view_cart(bound()).quote.lines.size());
    });
}

STDMETHODIMP CartObject::LineSku(long index, BSTR* sku) {
    return guard([&] {
        out_param(sku);
        *sku = to_bstr(line(index).sku);
    });
}

STDMETHODIMP CartObject::LineName(long index, BSTR* name) {
    return guard([&] {
        out_param(name);
        *name = to_bstr(line(index).name);
    });
}

STDMETHODIMP CartObject::LineQuantity(long index, long* quantity) {
    return guard([&] { *out_param(quantity) = line(index).qty; });
}

STDMETHODIMP CartObject::LineNetCents(long index, long* cents) {
    return guard([&] { *out_param(cents) = to_long(line(index).net); });
}

STDMETHODIMP CartObject::LineDiscountCents(long index, long* cents) {
    return guard([&] { *out_param(cents) = to_long(line(index).discount); });
}

STDMETHODIMP CartObject::get_SubtotalCents(long* cents) {
    return guard([&] { *out_param(cents) = to_long(shop().view_cart(bound()).quote.subtotal); });
}

STDMETHODIMP CartObject::get_DiscountCents(long* cents) {
    return guard([&] { *out_param(cents) = to_long(shop().view_cart(bound()).quote.discount); });
}

STDMETHODIMP CartObject::get_VatCents(long* cents) {
    return guard([&] { *out_param(cents) = to_long(shop().view_cart(bound()).quote.vat); });
}

STDMETHODIMP CartObject::get_TotalCents(long* cents) {
    return guard([&] { *out_param(cents) = to_long(shop().view_cart(bound()).quote.total); });
}

STDMETHODIMP CartObject::get_PromotionCode(BSTR* code) {
    return guard([&] {
        out_param(code);
        const auto q = shop().view_cart(bound()).quote;
        *code = to_bstr(q.promo_active ? q.promo_code : std::string());
    });
}

STDMETHODIMP CartObject::get_Notes(BSTR* notes) {
    return guard([&] {
        out_param(notes);
        std::string joined;
        for (const auto& n : shop().view_cart(bound()).quote.notes) {
            if (!joined.empty()) joined += '\n';
            joined += n;
        }
        *notes = to_bstr(joined);
    });
}

STDMETHODIMP CartObject::ToXml(BSTR* xml) {
    return guard([&] {
        out_param(xml);
        *xml = to_bstr(cart_xml(shop().view_cart(bound())));
    });
}

// ---- Order ----------------------------------------------------------------

const OrderRecord& OrderObject::loaded() const {
    if (!order_) throw ShopError(ErrorCode::UnknownOrder, "no order loaded: call Place or Load first");
    return *order_;
}

const OrderLine& OrderObject::line(long index) const {
    const auto& o = loaded();
    check_index(index, o.lines.size());
    return o.lines[static_cast<std::size_t>(index)];
}

STDMETHODIMP OrderObject::Place(BSTR cart_id, BSTR email, BSTR* order_id) {
    return guard([&] {
        out_param(order_id);
        order_ = shop().place_order(utf8(cart_id), utf8(email));
        *order_id = to_bstr(order_->order_id);
    });
}

STDMETHODIMP OrderObject::Load(BSTR order_id, VARIANT_BOOL* found) {
    return guard([&] {
        out_param(found);
        order_ = shop().find_order(utf8(order_id));
        *found = order_ ? VARIANT_TRUE : VARIANT_FALSE;
    });
}

STDMETHODIMP OrderObject::get_OrderId(BSTR* order_id) {
    return guard([&] {
        out_param(order_id);
        *order_id = to_bstr(loaded().order_id);
    });
}

STDMETHODIMP OrderObject::get_Email(BSTR* email) {
    return guard([&] {
        out_param(email);
        *email = to_bstr(loaded().email);
    });
}

STDMETHODIMP OrderObject::get_SubtotalCents(long* cents) {
    return guard([&] { *out_param(cents) = to_long(loaded().subtotal); });
}

STDMETHODIMP OrderObject::get_DiscountCents(long* cents) {
    return guard([&] { *out_param(cents) = to_long(loaded().discount); });
}

STDMETHODIMP OrderObject::get_VatCents(long* cents) {
    return guard([&] { *out_param(cents) = to_long(loaded().vat); });
}

STDMETHODIMP OrderObject::get_TotalCents(long* cents) {
    return guard([&] { *out_param(cents) = to_long(loaded().total); });
}

STDMETHODIMP OrderObject::get_LineCount(long* count) {
    return guard([&] { *out_param(count) = static_cast<long>(loaded().lines.size()); });
}

STDMETHODIMP OrderObject::LineSku(long index, BSTR* sku) {
    return guard([&] {
        out_param(sku);
        *sku = to_bstr(line(index).sku);
    });
}

STDMETHODIMP OrderObject::LineQuantity(long index, long* quantity) {
    return guard([&] { *out_param(quantity) = line(index).qty; });
}

STDMETHODIMP OrderObject::ToXml(BSTR* xml) {
    return guard([&] {
        out_param(xml);
        *xml = to_bstr(order_xml(loaded()));
    });
}

}  // namespace webcart::com
