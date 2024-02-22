//*********************************************************
//
//    Copyright (c) Microsoft. All rights reserved.
//    This code is licensed under the MIT License.
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
//    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//    PARTICULAR PURPOSE AND NONINFRINGEMENT.
//
//*********************************************************
//! @file
//! Utilities for making managing COM server easier

#ifndef __WIL_CPPWINRT_REGISTER_COM_SERVER_DEFINED
#define __WIL_CPPWINRT_REGISTER_COM_SERVER_DEFINED

#if _MSVC_LANG >= 201703L

#include <winrt/base.h>
#include <vector>

namespace wil::details
{
template <typename T>
struct CppWinRTClassFactory : winrt::implements<CppWinRTClassFactory<T>, IClassFactory, winrt::no_module_lock>
{
    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;

        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        return winrt::make_self<T>().as(iid, result);
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }
};

template <typename T = void, typename... Rest>
void register_com_server(std::vector<DWORD>& registrations)
{
    DWORD registration{};
    winrt::check_hresult(CoRegisterClassObject(
        winrt::guid_of<T>(), winrt::make<CppWinRTClassFactory<T>>().get(), CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &registration));
    registrations.push_back(registration);
    register_com_server<Rest...>(registrations);
}

template <>
void register_com_server<void>(std::vector<DWORD>&)
{
}
} // namespace wil::details

namespace wil
{
struct com_server_revoker
{
    com_server_revoker(std::vector<DWORD> registrations) : registrations(std::move(registrations))
    {
    }
    com_server_revoker(com_server_revoker const& that) = delete;
    void operator=(com_server_revoker const& that) = delete;
    ~com_server_revoker()
    {
        revoke();
    }
    void revoke()
    {
        for (auto&& registration : registrations)
        {
            winrt::check_hresult(CoRevokeClassObject(registration));
        }
        registrations.clear();
    }

private:
    std::vector<DWORD> registrations;
};

template <typename T, typename... Rest>
[[nodiscard]] com_server_revoker register_com_server()
{
    std::vector<DWORD> registrations;
    registrations.reserve(sizeof...(Rest) + 1);
    details::register_com_server<T, Rest...>(registrations);
    return com_server_revoker(std::move(registrations));
}
} // namespace wil

#endif

#endif