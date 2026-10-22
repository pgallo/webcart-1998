// C++ client of the registered local server: vtable calls through the
// type-library marshaler, HRESULT error mapping and IErrorInfo text.
// Run after webcart-server has been registered.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleauto.h>

#include <cstdio>
#include <string>

#include "webcart_h.h"

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                    \
    } while (0)

#define CHECK_HR(expr)                                                       \
    do {                                                                     \
        const HRESULT hr_ = (expr);                                          \
        ++g_checks;                                                          \
        if (FAILED(hr_)) {                                                   \
            ++g_failures;                                                    \
            std::printf("FAIL %s:%d: %s -> 0x%08lx\n", __FILE__, __LINE__, #expr, hr_); \
        }                                                                    \
    } while (0)

// Owns a BSTR; the marshaler needs real length-prefixed strings.
class Bstr {
public:
    Bstr() = default;
    explicit Bstr(const wchar_t* s) : b_(SysAllocString(s)) {}
    ~Bstr() { SysFreeString(b_); }
    Bstr(const Bstr&) = delete;
    Bstr& operator=(const Bstr&) = delete;
    BSTR* out() {
        SysFreeString(b_);
        b_ = nullptr;
        return &b_;
    }
    operator BSTR() const { return b_; }
    std::wstring str() const { return b_ ? std::wstring(b_, SysStringLen(b_)) : std::wstring(); }

private:
    BSTR b_ = nullptr;
};

template <class T>
struct Ptr {
    T* p = nullptr;
    ~Ptr() {
        if (p) p->Release();
    }
    T* operator->() const { return p; }
    void** out() { return reinterpret_cast<void**>(&p); }
};

constexpr HRESULT business_error(int code) {
    return static_cast<HRESULT>(0x80040200u + static_cast<unsigned>(code));
}

std::wstring last_error_description() {
    IErrorInfo* info = nullptr;
    if (GetErrorInfo(0, &info) != S_OK || !info) return L"";
    Bstr text;
    info->GetDescription(text.out());
    info->Release();
    return text.str();
}

}  // namespace

int main() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    {
        Ptr<ICatalog> catalog;
        HRESULT hr = CoCreateInstance(CLSID_Catalog, nullptr, CLSCTX_LOCAL_SERVER, IID_ICatalog, catalog.out());
        if (FAILED(hr)) {
            std::printf("CoCreateInstance(Catalog) failed: 0x%08lx (is the server registered?)\n", hr);
            return 1;
        }
        long count = 0;
        CHECK_HR(catalog->get_Count(&count));
        CHECK(count == 8);
        Bstr sku;
        CHECK_HR(catalog->SkuAt(0, sku.out()));
        CHECK(sku.str() == L"TEA-001");
        long price = 0;
        CHECK_HR(catalog->UnitPriceCents(sku, &price));
        CHECK(price == 650);
        Bstr missing(L"NOPE-1");
        CHECK(catalog->UnitPriceCents(missing, &price) == business_error(1));  // UNKNOWN_SKU
        CHECK(catalog->SkuAt(99, sku.out()) == business_error(11));             // BAD_REQUEST

        Ptr<ICart> cart;
        CHECK_HR(CoCreateInstance(CLSID_Cart, nullptr, CLSCTX_LOCAL_SERVER, IID_ICart, cart.out()));
        Bstr cart_id;
        CHECK_HR(cart->Create(cart_id.out()));
        CHECK(cart_id.str().size() == 17);

        Bstr tea(L"TEA-002"), mug(L"CUP-010"), kit(L"KIT-500"), code(L"WELCOME10");
        CHECK_HR(cart->AddItem(tea, 3));
        hr = cart->AddItem(kit, 3);  // only 2 in stock
        CHECK(hr == business_error(4));
        CHECK(last_error_description().rfind(L"OUT_OF_STOCK: ", 0) == 0);
        CHECK(cart->ApplyPromotion(code) == business_error(6));  // below the 30.00 minimum
        CHECK_HR(cart->AddItem(mug, 1));
        CHECK_HR(cart->ApplyPromotion(code));

        long lines = 0, subtotal = 0, discount = 0, vat = 0, total = 0;
        CHECK_HR(cart->get_LineCount(&lines));
        CHECK_HR(cart->get_SubtotalCents(&subtotal));
        CHECK_HR(cart->get_DiscountCents(&discount));
        CHECK_HR(cart->get_VatCents(&vat));
        CHECK_HR(cart->get_TotalCents(&total));
        CHECK(lines == 2);
        CHECK(subtotal == 3450);
        CHECK(discount == 345);
        CHECK(vat == 194 + 255);
        CHECK(total == 3554);

        Ptr<IOrder> order;
        CHECK_HR(CoCreateInstance(CLSID_Order, nullptr, CLSCTX_LOCAL_SERVER, IID_IOrder, order.out()));
        Bstr email(L"client.test@example.org"), order_id;
        CHECK_HR(order->Place(cart_id, email, order_id.out()));
        CHECK(order_id.str().rfind(L"ORD-", 0) == 0);
        long order_total = 0;
        CHECK_HR(order->get_TotalCents(&order_total));
        CHECK(order_total == total);

        Ptr<IOrder> lookup;
        CHECK_HR(CoCreateInstance(CLSID_Order, nullptr, CLSCTX_LOCAL_SERVER, IID_IOrder, lookup.out()));
        VARIANT_BOOL found = VARIANT_FALSE;
        CHECK_HR(lookup->Load(order_id, &found));
        CHECK(found == VARIANT_TRUE);
        CHECK_HR(lookup->get_LineCount(&lines));
        CHECK(lines == 2);

        CHECK_HR(cart->Open(cart_id, &found));
        CHECK(found == VARIANT_FALSE);  // a placed cart is closed

        // Late binding through IDispatch, as a script host would do it.
        DISPID dispid = 0;
        LPOLESTR name = const_cast<LPOLESTR>(L"Count");
        CHECK_HR(catalog->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispid));
        DISPPARAMS none = {nullptr, nullptr, 0, 0};
        VARIANT result;
        VariantInit(&result);
        CHECK_HR(catalog->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &none,
                                 &result, nullptr, nullptr));
        CHECK(result.vt == VT_I4 && result.lVal == 8);
        VariantClear(&result);
    }
    CoUninitialize();
    std::printf("com_client_test: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
