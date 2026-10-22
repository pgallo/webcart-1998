# Deploying on IIS with the COM business tier

This is the classic layout: ASP pages on IIS call the out-of-process COM
server; the servlet storefront reaches the same server through `gateway.asp`.
CI builds and tests the COM server and the ASP script logic, but does not
host the pages in IIS. Everything below is the manual procedure.

## 1. Build

From an MSVC x64 developer prompt (midl.exe and cl.exe on PATH):

```
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DWEBCART_BUILD_TESTS=OFF
cmake --build build
```

`build\com` then holds `webcart-server.exe`, `webcart.tlb` and `data\`.

## 2. Install and register the local server

Copy those three items to a fixed folder, for example `C:\WebCart\bin`, and
create a journal folder the server identity can write, for example
`C:\WebCart\journal`. From an elevated prompt:

```
C:\WebCart\bin\webcart-server.exe /RegServer -journal C:\WebCart\journal\orders.journal -idle 900
```

`regsvr32` registers in-process DLLs by calling their `DllRegisterServer`
export. An EXE server has no such export, so it registers itself with
`/RegServer` (and removes itself with `/UnregServer`). The options after
the switch are stored in the `LocalServer32` command line, so the process
COM launches gets them, followed by `-Embedding`.

Registration writes, under `HKLM\Software\Classes`:

| Key | Value |
| --- | --- |
| `CLSID\{clsid}\LocalServer32` | `"C:\WebCart\bin\webcart-server.exe" -journal ... -idle 900` |
| `CLSID\{clsid}\ProgID`, `VersionIndependentProgID` | `WebCart.Catalog.1`, `WebCart.Catalog` (and Cart, Order) |
| `CLSID\{clsid}\AppID`, `AppID\{appid}` | one AppID for the three classes: the place for DCOM security |
| `TypeLib\{libid}\1.0\0\win64` | path of `webcart.tlb` |
| `Interface\{iid}\ProxyStubClsid32` | the type library marshaler, written by `RegisterTypeLib` |

`/RegServerPerUser` writes the same keys under `HKCU` and needs no
administrator rights, which is enough for scripts run by the same user. IIS
worker processes and elevated callers do not see per-user registrations.

## 3. DCOM launch and access permissions

Run `dcomcnfg`, open Component Services, Computers, My Computer, DCOM Config,
"WebCart business tier", Properties:

- **Identity**: choose "This user" with a dedicated low-privilege account.
  With "The launching user", COM starts one server process per distinct
  caller identity, so ASP and a script would each get their own process and
  their own carts. A fixed identity gives one shared business tier.
- **Security, Launch and Activation Permissions**: grant the IIS application
  pool identity (for example `IIS AppPool\WebCart`) Local Launch and Local
  Activation.
- **Security, Access Permissions**: grant the same identity Local Access.
- For callers on another machine add Remote Launch, Remote Activation and
  Remote Access, register the type library on the client machine too (the
  marshaler needs it on both ends), and open RPC (TCP 135 plus the dynamic
  range) in the firewall. The servlet storefront does not need any of this:
  it talks HTTP to `gateway.asp`.

The server does not call `CoInitializeSecurity`, so these AppID settings are
the ones in force.

## 4. IIS and ASP

```
dism /online /enable-feature /featurename:IIS-ASP /all
```

- Create an application (for example `/webcart`) whose physical path is the
  repository's `asp` folder, in a 64-bit application pool with "No Managed
  Code".
- Session state is on by default. `web.config` hides `lib\` and `include\`
  and refuses `.inc`, `.vbs` and `.asa` downloads.
- Pages pull shared code with `<script runat="server" src="lib/...">` and
  `<!-- #include file="..." -->`, both relative, so "Enable Parent Paths"
  can stay off.
- Out-of-process components are allowed by default on current IIS versions;
  the server is called through `Server.CreateObject("WebCart.Cart")`.

## 5. The servlet storefront against IIS

Point the context parameter `webcart.gatewayUrl` in `WEB-INF/web.xml` (or
the system property of the same name) at
`http://iis-host/webcart/gateway.asp`. The request and response documents
are the same ones the portable `webcart-gateway` serves.

## Error codes seen by COM clients

Business errors are `FACILITY_ITF` HRESULTs with an `IErrorInfo`
description of the form `CODE: message`.

| HRESULT | Code |
| --- | --- |
| 0x80040201 | UNKNOWN_SKU |
| 0x80040202 | UNKNOWN_CART |
| 0x80040203 | INVALID_QUANTITY |
| 0x80040204 | OUT_OF_STOCK |
| 0x80040205 | UNKNOWN_PROMOTION |
| 0x80040206 | PROMOTION_NOT_APPLICABLE |
| 0x80040207 | EMPTY_CART |
| 0x80040208 | INVALID_EMAIL |
| 0x80040209 | UNKNOWN_ORDER |
| 0x8004020A | JOURNAL_FAILURE |
| 0x8004020B | BAD_REQUEST |
