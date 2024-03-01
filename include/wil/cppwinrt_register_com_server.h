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

#ifndef __WIL_CPPWINRT_REGISTER_COM_SERVER_INCLUDED
#define __WIL_CPPWINRT_REGISTER_COM_SERVER_INCLUDED

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
    // This push_back is no-throw as wil::register_com_server already reserves enough capacity
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
    com_server_revoker(std::vector<DWORD> registrations) noexcept : registrations(std::move(registrations))
    {
    }
    com_server_revoker(com_server_revoker const& that) = delete;
    void operator=(com_server_revoker const& that) = delete;
    ~com_server_revoker()
    {
        revoke();
    }
    void revoke() noexcept
    {
        for (auto&& registration : registrations)
        {
            // Ignore any revocation error, as per WRL implementation
            CoRevokeClassObject(registration);
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
    try
    {
        details::register_com_server<T, Rest...>(registrations);
        return com_server_revoker(std::move(registrations));
    }
    catch (...)
    {
        // Only registered server push its token to the list. Revoke them upon any error.
        // Note: technically if ctor of com_server_revoker throws, this is a double-move. However
        // the ctor is noexcept so this move and the move in the try block is mutually exclusive
        com_server_revoker(std::move(registrations)).revoke();
        throw;
    }
}
} // namespace wil

#endif