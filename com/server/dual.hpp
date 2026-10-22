#pragma once

#include <utility>

#include "module.hpp"

namespace webcart::com {

// IUnknown + IDispatch + ISupportErrorInfo for one dual interface, written
// out by hand instead of with a template library so every step is visible.
// IDispatch is implemented by delegating to the type library (DispInvoke),
// which calls straight back into the vtable: the script and C++ paths run
// the same method bodies.
template <class Itf>
class DualObject : public Itf, public ISupportErrorInfo {
public:
    DualObject() { lock_module(); }
    virtual ~DualObject() { unlock_module(); }
    DualObject(const DualObject&) = delete;
    DualObject& operator=(const DualObject&) = delete;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDispatch) ||
            IsEqualIID(riid, __uuidof(Itf))) {
            *ppv = static_cast<Itf*>(this);
        } else if (IsEqualIID(riid, IID_ISupportErrorInfo)) {
            *ppv = static_cast<ISupportErrorInfo*>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
    STDMETHODIMP_(ULONG) Release() override {
        const LONG n = InterlockedDecrement(&refs_);
        if (n == 0) delete this;
        return static_cast<ULONG>(n);
    }

    // ISupportErrorInfo
    STDMETHODIMP InterfaceSupportsErrorInfo(REFIID riid) override {
        return IsEqualIID(riid, __uuidof(Itf)) ? S_OK : S_FALSE;
    }

    // IDispatch
    STDMETHODIMP GetTypeInfoCount(UINT* count) override {
        if (!count) return E_POINTER;
        *count = 1;
        return S_OK;
    }
    STDMETHODIMP GetTypeInfo(UINT index, LCID, ITypeInfo** info) override {
        if (index != 0) return DISP_E_BADINDEX;
        return type_info_for(__uuidof(Itf), info);
    }
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR* names, UINT count, LCID, DISPID* ids) override {
        ITypeInfo* info = nullptr;
        HRESULT hr = type_info_for(__uuidof(Itf), &info);
        if (FAILED(hr)) return hr;
        hr = DispGetIDsOfNames(info, names, count, ids);
        info->Release();
        return hr;
    }
    STDMETHODIMP Invoke(DISPID id, REFIID riid, LCID, WORD flags, DISPPARAMS* params, VARIANT* result,
                        EXCEPINFO* excep, UINT* arg_err) override {
        if (!IsEqualIID(riid, IID_NULL)) return DISP_E_UNKNOWNINTERFACE;
        ITypeInfo* info = nullptr;
        HRESULT hr = type_info_for(__uuidof(Itf), &info);
        if (FAILED(hr)) return hr;
        // A failing vtable call comes back as DISP_E_EXCEPTION with the
        // HRESULT and IErrorInfo text copied into EXCEPINFO, which is what
        // VBScript surfaces as Err.Number and Err.Description.
        hr = DispInvoke(static_cast<Itf*>(this), info, id, flags, params, result, excep, arg_err);
        info->Release();
        return hr;
    }

protected:
    template <class F>
    HRESULT guard(F&& body) {
        return guarded(__uuidof(Itf), std::forward<F>(body));
    }

private:
    volatile LONG refs_ = 1;  // the creator's reference
};

template <class T>
class ClassFactory final : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    // Factories are static objects that live as long as the process.
    STDMETHODIMP_(ULONG) AddRef() override { return 2; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }

    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        T* object = new (std::nothrow) T();
        if (!object) return E_OUTOFMEMORY;
        const HRESULT hr = object->QueryInterface(riid, ppv);
        object->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        if (lock) lock_module();
        else unlock_module();
        return S_OK;
    }
};

}  // namespace webcart::com
