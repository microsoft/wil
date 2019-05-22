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

// WIL and C++/WinRT use two different exception types for communicating HRESULT failures. Thus, both libraries need to
// understand how to translate these exception types into the correct HRESULT values at the ABI boundary. Prior to
// C++/WinRT "2.0" this was accomplished by injecting the WINRT_EXTERNAL_CATCH_CLAUSE macro - that WIL defines below -
// into its exception handler (winrt::to_hresult). Starting with C++/WinRT "2.0" this mechanism has shifted to a global
// function pointer - winrt_to_hresult_handler - that WIL sets automatically when this header is included and
// 'RESULT_SUPPRESS_STATIC_INITIALIZERS' is not defined.

/// @cond
namespace wil::details
{
    // Since the C++/WinRT version macro is a string...
    inline constexpr int major_version_from_string(const char* versionString)
    {
        int result = 0;
        auto str = versionString;
        while ((*str >= '0') && (*str <= '9'))
        {
            result = result * 10 + (*str - '0');
            ++str;
        }

        return result;
    }
}
/// @endcond

#ifdef CPPWINRT_VERSION
// Prior to C++/WinRT "2.0" this header needed to be included before 'winrt/base.h' so that our definition of
// 'WINRT_EXTERNAL_CATCH_CLAUSE' would get picked up in the implementation of 'winrt::to_hresult'. This is no longer
// problematic, so only emit an error when using a version of C++/WinRT prior to 2.0
static_assert(::wil::details::major_version_from_string(CPPWINRT_VERSION) >= 2,
    "Please include wil/cppwinrt.h before including any C++/WinRT headers");
#endif

// NOTE: Will eventually be removed once C++/WinRT 2.0 use can be assumed
#ifdef WINRT_EXTERNAL_CATCH_CLAUSE
#define __WI_CONFLICTING_WINRT_EXTERNAL_CATCH_CLAUSE 1
#else
#define WINRT_EXTERNAL_CATCH_CLAUSE                                             \
    catch (const wil::ResultException& e)                                       \
    {                                                                           \
        return winrt::hresult_error(e.GetErrorCode(), winrt::to_hstring(e.what())).to_abi();  \
    }
#endif

#include "result_macros.h"
#include <winrt/base.h>

#if __WI_CONFLICTING_WINRT_EXTERNAL_CATCH_CLAUSE
static_assert(::wil::details::major_version_from_string(CPPWINRT_VERSION) >= 2,
    "C++/WinRT external catch clause already defined outside of WIL");
#endif

// In C++/WinRT 2.0 and beyond, this function pointer exists. In earlier versions it does not. It's much easier to avoid
// linker errors than it is to SFINAE on variable existence, so we declare the variable here, but are careful not to
// use it unless the version of C++/WinRT is high enough
extern std::int32_t(__stdcall* winrt_to_hresult_handler)(void*) noexcept;

/// @cond
namespace wil::details
{
    inline HRESULT __stdcall ResultFromCppWinRTException(
        _Inout_updates_opt_(debugStringChars) PWSTR debugString,
        _When_(debugString != nullptr, _Pre_satisfies_(debugStringChars > 0)) size_t debugStringChars) noexcept
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
/// @endcond

namespace wil
{
    inline std::int32_t __stdcall winrt_to_hresult(void* returnAddress) noexcept
    {
        // C++/WinRT only gives us the return address (caller), so pass along an empty 'DiagnosticsInfo' since we don't
        // have accurate file/line/etc. information
        return static_cast<std::int32_t>(details::ReportFailure_CaughtException(__R_DIAGNOSTICS_RA(DiagnosticsInfo{}, returnAddress), FailureType::Return));
    }

    inline void WilInitialize_CppWinRT()
    {
        details::g_pfnResultFromCaughtException_CppWinRt = details::ResultFromCppWinRTException;
        if constexpr (details::major_version_from_string(CPPWINRT_VERSION) >= 2)
        {
            WI_ASSERT(winrt_to_hresult_handler == nullptr);
            winrt_to_hresult_handler = winrt_to_hresult;
        }
    }

    /// @cond
    namespace details
    {
#ifndef RESULT_SUPPRESS_STATIC_INITIALIZERS
        WI_HEADER_INITITALIZATION_FUNCTION(WilInitialize_CppWinRT, []
        {
            ::wil::WilInitialize_CppWinRT();
            return 1;
        });
#endif
    }
    /// @endcond

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
