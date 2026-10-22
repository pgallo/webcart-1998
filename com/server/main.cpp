// webcart-server.exe: out-of-process COM server (LocalServer32) hosting the
// business tier. Reachable locally from ASP and scripts, and through DCOM
// from other machines once launch and access permissions allow it.
//
//   webcart-server /RegServer         [server options]   machine-wide (admin)
//   webcart-server /RegServerPerUser  [server options]   current user only
//   webcart-server /UnregServer | /UnregServerPerUser
//   webcart-server [-Embedding]       [server options]   serve (COM adds -Embedding)
//
// server options: -data DIR  -journal FILE  -idle SECONDS
// Options given at registration are stored in LocalServer32, so the
// COM-launched process sees them.

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

#include "objects.hpp"
#include "registration.hpp"

// No `using namespace webcart`: MIDL declares global classes named Catalog,
// Cart and Order for the coclasses, which would clash with the core types.
using namespace webcart::com;
using webcart::ShopConfig;

namespace {

std::wstring exe_path() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n < buf.size()) {
            buf.resize(n);
            return buf;
        }
        buf.resize(buf.size() * 2);
    }
}

std::wstring dir_of(const std::wstring& path) { return path.substr(0, path.find_last_of(L"\\/")); }

std::string narrow_path(const std::wstring& w) {
    // The portable core takes narrow paths, which the C runtime reads in the
    // ANSI code page on Windows.
    const int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n > 0 ? n - 1 : 0), '\0');
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring lower(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

std::wstring quote_arg(const std::wstring& a) {
    return a.find(L' ') == std::wstring::npos ? a : L"\"" + a + L"\"";
}

int usage() {
    std::fwprintf(stderr,
                  L"usage: webcart-server [/RegServer | /RegServerPerUser | /UnregServer |\n"
                  L"                       /UnregServerPerUser | -Embedding]\n"
                  L"                      [-data DIR] [-journal FILE] [-idle SECONDS]\n");
    return 2;
}

ClassFactory<CatalogObject> g_catalog_factory;
ClassFactory<CartObject> g_cart_factory;
ClassFactory<OrderObject> g_order_factory;

int serve(const std::wstring& data_dir, const std::wstring& journal, int idle_seconds,
          const std::wstring& tlb) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return 1;

    hr = load_type_library(tlb);
    if (FAILED(hr)) {
        std::fwprintf(stderr, L"cannot load type library %ls: 0x%08lx\n", tlb.c_str(), hr);
        return 1;
    }
    try {
        ShopConfig config;
        config.journal_path = narrow_path(journal);
        const std::string data = narrow_path(data_dir);
        init_shop(std::make_unique<webcart::Shop>(webcart::Catalog::load_csv(data + "\\catalog.csv"),
                                         webcart::load_promotions_csv(data + "\\promotions.csv"), config,
                                         std::make_shared<webcart::SystemClock>()));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "cannot start business tier: %s\n", e.what());
        return 1;
    }

    // Register suspended, then resume all at once, so a client never reaches
    // one class while another is still unregistered.
    struct Registration {
        const CLSID* clsid;
        IUnknown* factory;
        DWORD cookie;
    } regs[] = {{&CLSID_Catalog, &g_catalog_factory, 0},
                {&CLSID_Cart, &g_cart_factory, 0},
                {&CLSID_Order, &g_order_factory, 0}};
    for (auto& r : regs) {
        hr = CoRegisterClassObject(*r.clsid, r.factory, CLSCTX_LOCAL_SERVER,
                                   REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED, &r.cookie);
        if (FAILED(hr)) {
            std::fwprintf(stderr, L"CoRegisterClassObject failed: 0x%08lx\n", hr);
            return 1;
        }
    }
    CoResumeClassObjects();
    std::fwprintf(stderr, L"webcart-server ready (idle timeout %d s)\n", idle_seconds);

    // Carts live in this process, so it lingers after the last client
    // releases its objects instead of exiting at once and losing them.
    ULONGLONG idle_since = 0;
    for (;;) {
        Sleep(250);
        if (module_locks() > 0) {
            idle_since = 0;
            continue;
        }
        const ULONGLONG now = GetTickCount64();
        if (idle_since == 0) idle_since = now;
        if (now - idle_since >= static_cast<ULONGLONG>(idle_seconds) * 1000) break;
    }

    for (auto& r : regs) CoRevokeClassObject(r.cookie);
    CoUninitialize();
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const std::wstring exe = exe_path();
    const std::wstring dir = dir_of(exe);
    const std::wstring tlb = dir + L"\\webcart.tlb";
    std::wstring data_dir = dir + L"\\data";
    std::wstring journal = dir + L"\\orders.journal";
    int idle_seconds = 300;
    std::wstring mode = L"serve";
    std::wstring server_args;

    for (int i = 1; i < argc; ++i) {
        std::wstring a = lower(argv[i]);
        if (!a.empty() && a[0] == L'/') a[0] = L'-';
        if (a == L"-regserver" || a == L"-regserverperuser" || a == L"-unregserver" ||
            a == L"-unregserverperuser") {
            mode = a.substr(1);
        } else if (a == L"-embedding") {
            mode = L"serve";
        } else if ((a == L"-data" || a == L"-journal" || a == L"-idle") && i + 1 < argc) {
            const std::wstring value = argv[++i];
            if (a == L"-data") data_dir = value;
            else if (a == L"-journal") journal = value;
            else idle_seconds = static_cast<int>(std::wcstol(value.c_str(), nullptr, 10));
            server_args += L" " + a + L" " + quote_arg(value);
        } else {
            return usage();
        }
    }

    if (mode == L"serve") return serve(data_dir, journal, idle_seconds, tlb);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;
    const bool per_user = mode.size() > 7 && mode.compare(mode.size() - 7, 7, L"peruser") == 0;
    if (mode.rfind(L"regserver", 0) == 0) hr = register_server(per_user, exe, tlb, server_args);
    else hr = unregister_server(per_user, exe);
    CoUninitialize();
    std::wprintf(L"%ls (%ls): 0x%08lx %ls\n", mode.c_str(), per_user ? L"HKCU" : L"HKLM", hr,
                 SUCCEEDED(hr) ? L"ok" : L"failed");
    return SUCCEEDED(hr) ? 0 : 1;
}
