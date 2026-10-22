#include "module.hpp"

#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <new>

namespace webcart::com {

namespace {

std::unique_ptr<Shop> g_shop;
std::atomic<long> g_locks{0};
ITypeLib* g_typelib = nullptr;
std::mutex g_typeinfo_mutex;
struct GuidLess {
    bool operator()(const GUID& a, const GUID& b) const { return std::memcmp(&a, &b, sizeof(GUID)) < 0; }
};
std::map<GUID, ITypeInfo*, GuidLess> g_typeinfos;

}  // namespace

void init_shop(std::unique_ptr<Shop> shop) { g_shop = std::move(shop); }

Shop& shop() { return *g_shop; }

void lock_module() { ++g_locks; }
void unlock_module() { --g_locks; }
long module_locks() { return g_locks.load(); }

HRESULT load_type_library(const std::wstring& tlb_path) {
    return LoadTypeLibEx(tlb_path.c_str(), REGKIND_NONE, &g_typelib);
}

HRESULT type_info_for(REFIID iid, ITypeInfo** out) {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (!g_typelib) return E_UNEXPECTED;
    std::lock_guard lock(g_typeinfo_mutex);
    auto it = g_typeinfos.find(iid);
    if (it == g_typeinfos.end()) {
        ITypeInfo* info = nullptr;
        const HRESULT hr = g_typelib->GetTypeInfoOfGuid(iid, &info);
        if (FAILED(hr)) return hr;
        it = g_typeinfos.emplace(iid, info).first;  // cache keeps one reference
    }
    it->second->AddRef();
    *out = it->second;
    return S_OK;
}

std::string utf8(BSTR s) {
    if (!s) return {};
    const int len = static_cast<int>(SysStringLen(s));
    if (len == 0) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, s, len, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, len, out.data(), bytes, nullptr, nullptr);
    return out;
}

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

BSTR to_bstr(const std::string& s) {
    const std::wstring w = widen(s);
    BSTR b = SysAllocStringLen(w.data(), static_cast<UINT>(w.size()));
    if (!b) throw std::bad_alloc();
    return b;
}

HRESULT error_from_current_exception(REFIID iid) {
    HRESULT hr = E_FAIL;
    std::string message;
    try {
        throw;
    } catch (const ShopError& e) {
        hr = static_cast<HRESULT>(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x200 + static_cast<int>(e.code())));
        message = std::string(code_name(e.code())) + ": " + e.what();
    } catch (const NullOutPointer&) {
        return E_POINTER;
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    } catch (const std::exception& e) {
        message = std::string("INTERNAL: ") + e.what();
    } catch (...) {
        hr = E_UNEXPECTED;
        message = "INTERNAL: unknown failure";
    }

    ICreateErrorInfo* create = nullptr;
    if (SUCCEEDED(CreateErrorInfo(&create))) {
        std::wstring description = widen(message);
        std::wstring source = L"WebCart";
        create->SetGUID(iid);
        create->SetSource(source.data());
        create->SetDescription(description.data());
        IErrorInfo* info = nullptr;
        if (SUCCEEDED(create->QueryInterface(IID_IErrorInfo, reinterpret_cast<void**>(&info)))) {
            SetErrorInfo(0, info);
            info->Release();
        }
        create->Release();
    }
    return hr;
}

}  // namespace webcart::com
