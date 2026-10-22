#pragma once

#include <new>
#include <optional>
#include <string>

#include "dual.hpp"
#include "webcart/gateway_protocol.hpp"

namespace webcart::com {

class CatalogObject final : public DualObject<ICatalog> {
public:
    STDMETHODIMP get_Count(long* count) override;
    STDMETHODIMP SkuAt(long index, BSTR* sku) override;
    STDMETHODIMP ProductName(BSTR sku, BSTR* name) override;
    STDMETHODIMP Category(BSTR sku, BSTR* category) override;
    STDMETHODIMP UnitPriceCents(BSTR sku, long* cents) override;
    STDMETHODIMP VatRateBp(BSTR sku, long* basis_points) override;
    STDMETHODIMP Available(BSTR sku, long* units) override;
    STDMETHODIMP ToXml(BSTR* xml) override;
};

// A cart object is a handle: Create or Open binds it to a server-side cart id.
class CartObject final : public DualObject<ICart> {
public:
    STDMETHODIMP Create(BSTR* cart_id) override;
    STDMETHODIMP Open(BSTR cart_id, VARIANT_BOOL* found) override;
    STDMETHODIMP get_CartId(BSTR* cart_id) override;
    STDMETHODIMP AddItem(BSTR sku, long quantity) override;
    STDMETHODIMP SetQuantity(BSTR sku, long quantity) override;
    STDMETHODIMP RemoveItem(BSTR sku) override;
    STDMETHODIMP ApplyPromotion(BSTR code) override;
    STDMETHODIMP get_LineCount(long* count) override;
    STDMETHODIMP LineSku(long index, BSTR* sku) override;
    STDMETHODIMP LineName(long index, BSTR* name) override;
    STDMETHODIMP LineQuantity(long index, long* quantity) override;
    STDMETHODIMP LineNetCents(long index, long* cents) override;
    STDMETHODIMP LineDiscountCents(long index, long* cents) override;
    STDMETHODIMP get_SubtotalCents(long* cents) override;
    STDMETHODIMP get_DiscountCents(long* cents) override;
    STDMETHODIMP get_VatCents(long* cents) override;
    STDMETHODIMP get_TotalCents(long* cents) override;
    STDMETHODIMP get_PromotionCode(BSTR* code) override;
    STDMETHODIMP get_Notes(BSTR* notes) override;
    STDMETHODIMP ToXml(BSTR* xml) override;

private:
    const std::string& bound() const;
    QuoteLine line(long index) const;
    std::string cart_id_;
};

class OrderObject final : public DualObject<IOrder> {
public:
    STDMETHODIMP Place(BSTR cart_id, BSTR email, BSTR* order_id) override;
    STDMETHODIMP Load(BSTR order_id, VARIANT_BOOL* found) override;
    STDMETHODIMP get_OrderId(BSTR* order_id) override;
    STDMETHODIMP get_Email(BSTR* email) override;
    STDMETHODIMP get_SubtotalCents(long* cents) override;
    STDMETHODIMP get_DiscountCents(long* cents) override;
    STDMETHODIMP get_VatCents(long* cents) override;
    STDMETHODIMP get_TotalCents(long* cents) override;
    STDMETHODIMP get_LineCount(long* count) override;
    STDMETHODIMP LineSku(long index, BSTR* sku) override;
    STDMETHODIMP LineQuantity(long index, long* quantity) override;
    STDMETHODIMP ToXml(BSTR* xml) override;

private:
    const OrderRecord& loaded() const;
    const OrderLine& line(long index) const;
    std::optional<OrderRecord> order_;
};

}  // namespace webcart::com
