#include "registration.hpp"

#include <cstdio>

namespace webcart::com {

namespace {

// AppID groups the three classes under one server identity, which is where
// DCOM launch/access permissions and RunAs are configured.
const GUID kAppId = {0xCC1B48E3, 0x549D, 0x418A, {0xA2, 0x30, 0x86, 0xAF, 0x72, 0x62, 0xB3, 0x60}};

struct ClassEntry {
    const CLSID* clsid;
    const wchar_t* description;
    const wchar_t* prog_id;
};

const ClassEntry kClasses[] = {
    {&CLSID_Catalog, L"WebCart Catalog", L"WebCart.Catalog"},
    {&CLSID_Cart, L"WebCart Cart", L"WebCart.Cart"},
    {&CLSID_Order, L"WebCart Order", L"WebCart.Order"},
};

std::wstring guid_string(REFGUID guid) {
    wchar_t buf[64] = {};
    StringFromGUID2(guid, buf, 64);
    return buf;
}

class Writer {
public:
    explicit Writer(HKEY root) : root_(root) {}
    void set(const std::wstring& key, const wchar_t* name, const std::wstring& value) {
        const LSTATUS st = RegSetKeyValueW(root_, key.c_str(), name, REG_SZ, value.c_str(),
                                           static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        if (st != ERROR_SUCCESS && status_ == ERROR_SUCCESS) {
            status_ = st;
            std::fwprintf(stderr, L"cannot write %ls: error %ld\n", key.c_str(), st);
        }
    }
    HRESULT result() const { return HRESULT_FROM_WIN32(status_); }

private:
    HKEY root_;
    LSTATUS status_ = ERROR_SUCCESS;
};

HRESULT open_classes_root(bool per_user, HKEY* key) {
    const LSTATUS st = RegCreateKeyExW(per_user ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
                                       L"Software\\Classes", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr,
                                       key, nullptr);
    return HRESULT_FROM_WIN32(st);
}

std::wstring file_name(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

}  // namespace

HRESULT register_server(bool per_user, const std::wstring& exe_path, const std::wstring& tlb_path,
                        const std::wstring& server_args) {
    HKEY root = nullptr;
    HRESULT hr = open_classes_root(per_user, &root);
    if (FAILED(hr)) return hr;

    Writer w(root);
    const std::wstring app_id = guid_string(kAppId);
    const std::wstring lib_id = guid_string(LIBID_WebCartLib);
    w.set(L"AppID\\" + app_id, nullptr, L"WebCart business tier");
    w.set(L"AppID\\" + file_name(exe_path), L"AppID", app_id);

    for (const auto& c : kClasses) {
        const std::wstring clsid = guid_string(*c.clsid);
        const std::wstring key = L"CLSID\\" + clsid;
        const std::wstring versioned = std::wstring(c.prog_id) + L".1";
        w.set(key, nullptr, c.description);
        w.set(key, L"AppID", app_id);
        // COM appends -Embedding when it launches the server for a client.
        w.set(key + L"\\LocalServer32", nullptr, L"\"" + exe_path + L"\"" + server_args);
        w.set(key + L"\\ProgID", nullptr, versioned);
        w.set(key + L"\\VersionIndependentProgID", nullptr, c.prog_id);
        w.set(key + L"\\TypeLib", nullptr, lib_id);
        w.set(c.prog_id, nullptr, c.description);
        w.set(std::wstring(c.prog_id) + L"\\CLSID", nullptr, clsid);
        w.set(std::wstring(c.prog_id) + L"\\CurVer", nullptr, versioned);
        w.set(versioned, nullptr, c.description);
        w.set(versioned + L"\\CLSID", nullptr, clsid);
    }
    RegCloseKey(root);
    if (FAILED(w.result())) return w.result();

    // Registering the type library also writes the Interface keys that point
    // the dual interfaces at the universal (type library) marshaler.
    ITypeLib* typelib = nullptr;
    hr = LoadTypeLibEx(tlb_path.c_str(), REGKIND_NONE, &typelib);
    if (FAILED(hr)) return hr;
    std::wstring path = tlb_path;
    hr = per_user ? RegisterTypeLibForUser(typelib, path.data(), nullptr)
                  : RegisterTypeLib(typelib, path.data(), nullptr);
    typelib->Release();
    return hr;
}

HRESULT unregister_server(bool per_user, const std::wstring& exe_path) {
    HKEY root = nullptr;
    HRESULT hr = open_classes_root(per_user, &root);
    if (FAILED(hr)) return hr;
    for (const auto& c : kClasses) {
        RegDeleteTreeW(root, (L"CLSID\\" + guid_string(*c.clsid)).c_str());
        RegDeleteTreeW(root, c.prog_id);
        RegDeleteTreeW(root, (std::wstring(c.prog_id) + L".1").c_str());
    }
    RegDeleteTreeW(root, (L"AppID\\" + guid_string(kAppId)).c_str());
    RegDeleteTreeW(root, (L"AppID\\" + file_name(exe_path)).c_str());
    RegCloseKey(root);

    const SYSKIND kind = sizeof(void*) == 8 ? SYS_WIN64 : SYS_WIN32;
    hr = per_user ? UnRegisterTypeLibForUser(LIBID_WebCartLib, 1, 0, LOCALE_NEUTRAL, kind)
                  : UnRegisterTypeLib(LIBID_WebCartLib, 1, 0, LOCALE_NEUTRAL, kind);
    // Already gone is fine for an uninstall.
    return hr == TYPE_E_REGISTRYACCESS || hr == TYPE_E_LIBNOTREGISTERED ? S_OK : hr;
}

}  // namespace webcart::com
