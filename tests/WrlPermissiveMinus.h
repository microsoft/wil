#pragma once

// This is a temporary workaround since the latest update of MSVC has uncovered additional /permissive- issues with WRL.
// In particular, the call to CustomQueryInterface needs to be qualified

#include <wrl/implements.h>
#undef InspectableClass

#define InspectableClass(runtimeClassName, trustLevel) \
    public: \
        static _Null_terminated_ const wchar_t* STDMETHODCALLTYPE InternalGetRuntimeClassName() throw() \
        { \
            static_assert((RuntimeClassT::ClassFlags::value & ::Microsoft::WRL::WinRtClassicComMix) == ::Microsoft::WRL::WinRt || \
                (RuntimeClassT::ClassFlags::value & ::Microsoft::WRL::WinRtClassicComMix) == ::Microsoft::WRL::WinRtClassicComMix, \
                    "'InspectableClass' macro must not be used with ClassicCom clasess."); \
            static_assert(__is_base_of(::Microsoft::WRL::Details::RuntimeClassBase, RuntimeClassT), "'InspectableClass' macro can only be used with ::Windows::WRL::RuntimeClass types"); \
            static_assert(!__is_base_of(IActivationFactory, RuntimeClassT), "Incorrect usage of IActivationFactory interface. Make sure that your RuntimeClass doesn't implement IActivationFactory interface use ::Windows::WRL::ActivationFactory instead or 'InspectableClass' macro is not used on ::Windows::WRL::ActivationFactory"); \
            return runtimeClassName; \
        } \
        static ::TrustLevel STDMETHODCALLTYPE InternalGetTrustLevel() throw() \
        { \
            return trustLevel; \
        } \
        STDMETHOD(GetRuntimeClassName)(_Out_ HSTRING* runtimeName) \
        { \
            *runtimeName = nullptr; \
            HRESULT hr = S_OK; \
            auto name = InternalGetRuntimeClassName(); \
            if (name != nullptr) \
            { \
                hr = ::WindowsCreateString(name, static_cast<UINT32>(::wcslen(name)), runtimeName); \
            } \
            return hr; \
        } \
        STDMETHOD(GetTrustLevel)(_Out_ ::TrustLevel* trustLvl) \
        { \
            *trustLvl = trustLevel; \
            return S_OK; \
        } \
        STDMETHOD(GetIids)(_Out_ ULONG *iidCount, \
            _When_(*iidCount == 0, _At_(*iids, _Post_null_)) \
            _When_(*iidCount > 0, _At_(*iids, _Post_notnull_)) \
            _Result_nullonfailure_ IID **iids) \
        { \
            return RuntimeClassT::GetIids(iidCount, iids); \
        } \
        STDMETHOD(QueryInterface)(REFIID riid, _Outptr_result_nullonfailure_ void **ppvObject) \
        { \
            bool handled = false; \
            HRESULT hr = RuntimeClassT::CustomQueryInterface(riid, ppvObject, &handled); \
            if (FAILED(hr) || handled) return hr; \
            return RuntimeClassT::QueryInterface(riid, ppvObject); \
        } \
        STDMETHOD_(ULONG, Release)() \
        { \
            return RuntimeClassT::Release(); \
        } \
        STDMETHOD_(ULONG, AddRef)() \
        { \
            return RuntimeClassT::AddRef(); \
        } \
    private:
