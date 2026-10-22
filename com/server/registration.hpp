#pragma once

#include <string>

#include "module.hpp"

namespace webcart::com {

// per_user writes HKCU\Software\Classes (no administrator rights needed);
// otherwise HKLM\Software\Classes, which is what IIS and remote DCOM clients
// see. server_args is appended to the LocalServer32 command line.
HRESULT register_server(bool per_user, const std::wstring& exe_path, const std::wstring& tlb_path,
                        const std::wstring& server_args);
HRESULT unregister_server(bool per_user, const std::wstring& exe_path);

}  // namespace webcart::com
