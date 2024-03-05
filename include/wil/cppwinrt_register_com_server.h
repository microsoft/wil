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
#include <wil/resource.h>
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
void register_com_server(std::vector<wil::unique_com_class_object_cookie>& registrations)
{
    DWORD registration{};
    winrt::check_hresult(CoRegisterClassObject(
        winrt::guid_of<T>(), winrt::make<CppWinRTClassFactory<T>>().get(), CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &registration));
    // This emplace_back is no-throw as wil::register_com_server already reserves enough capacity
    registrations.emplace_back(registration);
    register_com_server<Rest...>(registrations);
}

template <>
void register_com_server<void>(std::vector<unique_com_class_object_cookie>&)
{
}
} // namespace wil::details

namespace wil
{
template <typename T, typename... Rest>
[[nodiscard]] std::vector<wil::unique_com_class_object_cookie> register_com_server()
{
    std::vector<wil::unique_com_class_object_cookie> registrations;
    registrations.reserve(sizeof...(Rest) + 1);
    try
    {
        details::register_com_server<T, Rest...>(registrations);
        // C++17 doesn't provide guaranteed copy ellision, but the copy should be elided nonetheless.
        return registrations;
    }
    catch (...)
    {
        // Only registered server push its token to the list. Revoke them upon any error.
        registrations.clear();
        throw;
    }
}
} // namespace wil

#endif