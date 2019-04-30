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
#ifndef __WIL_CPPWINRT_INCLUDED
#define __WIL_CPPWINRT_INCLUDED

#include "common.h"
#include <windows.h>
#include <unknwn.h>
#include <hstring.h>

#ifdef CPPWINRT_VERSION
#error Please include wil/cppwinrt.h before including any C++/WinRT headers
#endif

#ifdef __WIL_RESULTMACROS_INCLUDED
#error Please include wil/cppwinrt.h before including result_macros.h
#endif

#ifdef WINRT_EXTERNAL_CATCH_CLAUSE
#error C++/WinRT external catch clause already defined outside of WIL
#endif

// WIL and C++/WinRT use two different exception types for communicating HRESULT failures. Thus, both libraries need to
// understand how to translate these exception types into the correct HRESULT values at the ABI boundary. C++/WinRT
// accomplishes this by injecting the WINRT_EXTERNAL_CATCH_CLAUSE macro - that WIL defines below - into its exception
// handler. WIL accomplishes this by detecting this file's inclusion in result_macros.h and modifies its behavior to
// account for the C++/WinRT exception type.

#define WINRT_EXTERNAL_CATCH_CLAUSE                                             \
    catch (const wil::ResultException& e)                                       \
    {                                                                           \
        return winrt::hresult_error(e.GetErrorCode(), winrt::to_hstring(e.what())).to_abi();  \
    }

namespace wil::details
{
    // Due to header dependency issues, result_macros.h cannot reference winrt::hresult_error, so instead declare
    // functions that we can define after including base.h
    HRESULT __stdcall ResultFromCppWinRTException(
        _Inout_updates_opt_(debugStringChars) PWSTR debugString,
        _When_(debugString != nullptr, _Pre_satisfies_(debugStringChars > 0)) size_t debugStringChars) WI_NOEXCEPT;
}

#include "result_macros.h"
#include <winrt/base.h>

namespace wil::details
{
    inline HRESULT __stdcall ResultFromCppWinRTException(
        _Inout_updates_opt_(debugStringChars) PWSTR debugString,
        _When_(debugString != nullptr, _Pre_satisfies_(debugStringChars > 0)) size_t debugStringChars) WI_NOEXCEPT
    {
        try
        {
            throw;
        }
        catch (const winrt::hresult_error& e)
        {
            if (debugString)
            {
                StringCchPrintfW(debugString, debugStringChars, L"winrt::hresult_error: %ls", e.message().c_str());
            }

            return e.to_abi();
        }
        catch (...)
        {
            // Not a C++/WinRT exception; let the caller decide what to do
            return S_OK;
        }
    }
}

namespace wil
{
    // Provides an overload of verify_hresult so that the WIL macros can recognize winrt::hresult as a valid "hresult" type.
    inline long verify_hresult(winrt::hresult hr) noexcept
    {
        return hr;
    }

    // Provides versions of get_abi and put_abi for genericity that directly use HSTRING for convenience.
    template <typename T>
    auto get_abi(T const& object) noexcept
    {
        return winrt::get_abi(object);
    }

    inline auto get_abi(winrt::hstring const& object) noexcept
    {
        return static_cast<HSTRING>(winrt::get_abi(object));
    }

    template <typename T>
    auto put_abi(T& object) noexcept
    {
        return winrt::put_abi(object);
    }

    inline auto put_abi(winrt::hstring& object) noexcept
    {
        return reinterpret_cast<HSTRING*>(winrt::put_abi(object));
    }
}

#endif // __WIL_CPPWINRT_INCLUDED
