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
//! WIL Error Handling Helpers: support for interoperability between WIL and C++/WinRT exception-to-HRESULT logic.
#ifndef __WIL_CPPWINRT_INCLUDED
#define __WIL_CPPWINRT_INCLUDED

#include "common.h"
#ifdef __MINGW32__
#include <windows.h>
#else
#include <Windows.h>
#endif
#include <unknwn.h>
#include <inspectable.h>
#include <hstring.h>

// WIL and C++/WinRT use two different exception types for communicating HRESULT failures. Thus, both libraries need to
// understand how to translate these exception types into the correct HRESULT values at the ABI boundary. Prior to
// C++/WinRT "2.0" this was accomplished by injecting the WINRT_EXTERNAL_CATCH_CLAUSE macro - that WIL defines below -
// into its exception handler (winrt::to_hresult). Starting with C++/WinRT "2.0" this mechanism has shifted to a global
// function pointer - winrt_to_hresult_handler - that WIL sets automatically when this header is included and
// 'CPPWINRT_SUPPRESS_STATIC_INITIALIZERS' is not defined.

/// @cond
namespace wil::details
{
// Since the C++/WinRT version macro is a string...
// For example: "2.0.221104.6"
constexpr int version_from_string(const char* versionString)
{
    int result = 0;
    while ((*versionString >= '0') && (*versionString <= '9'))
    {
        result = result * 10 + (*versionString - '0');
        ++versionString;
    }

    return result;
}

constexpr int major_version_from_string(const char* versionString)
{
    return version_from_string(versionString);
}

constexpr int minor_version_from_string(const char* versionString)
{
    int dotCount = 0;
    while ((*versionString != '\0'))
    {
        if (*versionString == '.')
        {
            ++dotCount;
        }

        ++versionString;
        if (dotCount == 2)
        {
            return version_from_string(versionString);
        }
    }

    return 0;
}
} // namespace wil::details
/// @endcond

#ifdef CPPWINRT_VERSION
// Prior to C++/WinRT "2.0" this header needed to be included before 'winrt/base.h' so that our definition of
// 'WINRT_EXTERNAL_CATCH_CLAUSE' would get picked up in the implementation of 'winrt::to_hresult'. This is no longer
// problematic, so only emit an error when using a version of C++/WinRT prior to 2.0
static_assert(::wil::details::major_version_from_string(CPPWINRT_VERSION) >= 2, "Please include wil/cppwinrt.h before including any C++/WinRT headers");
#endif

// NOTE: Will eventually be removed once C++/WinRT 2.0 use can be assumed
/// @cond
#ifdef WINRT_EXTERNAL_CATCH_CLAUSE
#define __WI_CONFLICTING_WINRT_EXTERNAL_CATCH_CLAUSE 1
#else
#define WINRT_EXTERNAL_CATCH_CLAUSE \
    catch (const wil::ResultException& e) \
    { \
        return winrt::hresult_error(e.GetErrorCode(), winrt::to_hstring(e.what())).to_abi(); \
    }
#endif
/// @endcond

#include "result_macros.h"
#include <winrt/base.h>

#if __WI_CONFLICTING_WINRT_EXTERNAL_CATCH_CLAUSE
static_assert(::wil::details::major_version_from_string(CPPWINRT_VERSION) >= 2, "C++/WinRT external catch clause already defined outside of WIL");
#endif

/// @cond
// In C++/WinRT 2.0 and beyond, this function pointer exists. In earlier versions it does not. It's much easier to avoid
// linker errors than it is to SFINAE on variable existence, so we declare the variable here, but are careful not to
// use it unless the version of C++/WinRT is high enough
// NOLINTBEGIN(readability-redundant-declaration)
extern std::int32_t(__stdcall* winrt_to_hresult_handler)(void*) noexcept;

// The same is true with this function pointer as well, except that the version must be 2.X or higher.
extern void(__stdcall* winrt_throw_hresult_handler)(uint32_t, char const*, char const*, void*, winrt::hresult const) noexcept;
// NOLINTEND(readability-redundant-declaration)
/// @endcond

/// @cond
namespace wil::details
{
inline void MaybeGetExceptionString(
    const winrt::hresult_error& exception,
    _Out_writes_opt_(debugStringChars) PWSTR debugString,
    _When_(debugString != nullptr, _Pre_satisfies_(debugStringChars > 0)) size_t debugStringChars)
{
    if (debugString)
    {
        StringCchPrintfW(debugString, debugStringChars, L"winrt::hresult_error: %ls", exception.message().c_str());
    }
}

inline HRESULT __stdcall ResultFromCaughtException_CppWinRt(
    _Inout_updates_opt_(debugStringChars) PWSTR debugString,
    _When_(debugString != nullptr, _Pre_satisfies_(debugStringChars > 0)) size_t debugStringChars,
    _Inout_ bool* isNormalized) noexcept
{
    if (g_pfnResultFromCaughtException)
    {
        try
        {
            throw;
        }
        catch (const ResultException& exception)
        {
            *isNormalized = true;
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return exception.GetErrorCode();
        }
        catch (const winrt::hresult_error& exception)
        {
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return exception.to_abi();
        }
        catch (const std::bad_alloc& exception)
        {
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return E_OUTOFMEMORY;
        }
        catch (const std::out_of_range& exception)
        {
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return E_BOUNDS;
        }
        catch (const std::invalid_argument& exception)
        {
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return E_INVALIDARG;
        }
        catch (...)
        {
            auto hr = RecognizeCaughtExceptionFromCallback(debugString, debugStringChars);
            if (FAILED(hr))
            {
                return hr;
            }
        }
    }
    else
    {
        try
        {
            throw;
        }
        catch (const ResultException& exception)
        {
            *isNormalized = true;
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return exception.GetErrorCode();
        }
        catch (const winrt::hresult_error& exception)
        {
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return exception.to_abi();
        }
        catch (const std::bad_alloc& exception)
        {
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return E_OUTOFMEMORY;
        }
        catch (const std::out_of_range& exception)
        {
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return E_BOUNDS;
        }
        catch (const std::invalid_argument& exception)
        {
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return E_INVALIDARG;
        }
        catch (const std::exception& exception)
        {
            MaybeGetExceptionString(exception, debugString, debugStringChars);
            return HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION);
        }
        catch (...)
        {
            // Fall through to returning 'S_OK' below
        }
    }

    // Tell the caller that we were unable to map the exception by succeeding...
    return S_OK;
}
} // namespace wil::details
/// @endcond

namespace wil
{
/// @cond
inline std::int32_t __stdcall winrt_to_hresult(void* returnAddress) noexcept
{
    // C++/WinRT only gives us the return address (caller), so pass along an empty 'DiagnosticsInfo' since we don't
    // have accurate file/line/etc. information
    return static_cast<std::int32_t>(
        details::ReportFailure_CaughtException<FailureType::Return>(__R_DIAGNOSTICS_RA(DiagnosticsInfo{}, returnAddress)));
}

inline void __stdcall winrt_throw_hresult(
    uint32_t lineNumber, char const* fileName, char const* functionName, void* returnAddress, winrt::hresult const result) noexcept
{
    void* callerReturnAddress{nullptr};
    PCSTR code{nullptr};
    wil::details::ReportFailure_Hr<FailureType::Log>(__R_FN_CALL_FULL __R_COMMA result);
}

inline void WilInitialize_CppWinRT()
{
    details::g_pfnResultFromCaughtException_CppWinRt = details::ResultFromCaughtException_CppWinRt;
    if constexpr (details::major_version_from_string(CPPWINRT_VERSION) >= 2)
    {
        WI_ASSERT(winrt_to_hresult_handler == nullptr);
        winrt_to_hresult_handler = winrt_to_hresult;

        if constexpr (details::minor_version_from_string(CPPWINRT_VERSION) >= 210122)
        {
            WI_ASSERT(winrt_throw_hresult_handler == nullptr);
            winrt_throw_hresult_handler = winrt_throw_hresult;
        }
    }
}
/// @endcond

/// @cond
namespace details
{
#ifndef CPPWINRT_SUPPRESS_STATIC_INITIALIZERS
    WI_ODR_PRAGMA("CPPWINRT_SUPPRESS_STATIC_INITIALIZERS", "0")
    WI_HEADER_INITIALIZATION_FUNCTION(WilInitialize_CppWinRT, [] {
        ::wil::WilInitialize_CppWinRT();
        return 1;
    });
#else
    WI_ODR_PRAGMA("CPPWINRT_SUPPRESS_STATIC_INITIALIZERS", "1")
#endif
} // namespace details
/// @endcond

//! Lets WIL's error-handling macros accept a `winrt::hresult` where they expect an `HRESULT`.
//! This overload is found by the `RETURN_IF_FAILED` / `THROW_IF_FAILED` family (and friends) so a `winrt::hresult` result can be
//! checked directly, without first converting it to an `HRESULT`.
//! @param hr The `winrt::hresult` to validate.
//! @return `hr`, as an `HRESULT`-compatible value.
inline long verify_hresult(winrt::hresult hr) noexcept
{
    return hr;
}

//! Returns the raw ABI representation of a C++/WinRT projected object, for passing across the ABI boundary in generic code.
//! This overload set exists mainly because `winrt::get_abi` no longer references any Windows SDK types: for a `winrt::hstring` it
//! returns a bare `void*` rather than an `HSTRING`, so WIL provides these as an alternative that at least knows about `HSTRING`
//! (the `winrt::hstring` overload returns the concrete `HSTRING`). For every other type it simply forwards to `winrt::get_abi`.
//! For a projected interface or runtime class `winrt::impl::abi_t<T>` resolves to a `void*` ABI interface pointer; for a
//! projected value type it is instead the `winrt::impl::abi_t<T>` ABI struct, by value (layout-compatible with, but distinct
//! from, the classic/SDK `ABI::` type). Paired with @ref put_abi.
//! @tparam T A C++/WinRT projected type.
//! @param object The object whose ABI representation to return.
//! @return For an interface or runtime class, a borrowed (not AddRef'd) `void*` interface pointer; for a value type, the
//!         `winrt::impl::abi_t<T>` ABI struct by value.
template <typename T>
auto get_abi(T const& object) noexcept
{
    return winrt::get_abi(object);
}

//! Returns the raw `HSTRING` that backs a `winrt::hstring`.
//! @param object The string whose `HSTRING` to return.
//! @return The `HSTRING` backing `object` (may be null for an empty string).
inline auto get_abi(winrt::hstring const& object) noexcept
{
    return static_cast<HSTRING>(winrt::get_abi(object));
}

//! Returns a null-terminated wide-character pointer to a `winrt::hstring`'s buffer.
//! This is the `winrt::hstring` overload of WIL's generic `str_raw_ptr` set, which normalizes the string types WIL supports (raw
//! `PCWSTR`, `std::wstring`, `HSTRING`, and so on) to a `PCWSTR` for use by WIL's generic string helpers.
//! @param str The string to read from.
//! @return A `PCWSTR` to `str`'s contents, valid for as long as `str` is alive and unmodified.
inline auto str_raw_ptr(const winrt::hstring& str) noexcept
{
    return str.c_str();
}

//! Returns the address of a C++/WinRT object's ABI pointer, for receiving it as an out-parameter in generic code.
//! Like @ref get_abi, this exists mainly because `winrt::put_abi` no longer references any Windows SDK types: for a
//! `winrt::hstring` it yields a bare `void**` rather than an `HSTRING*`, so the `winrt::hstring` overload here yields the
//! concrete `HSTRING*`. For every other type it forwards to `winrt::put_abi`. For a projected interface or runtime class the
//! result is `void**` (the address of the object's `void*` ABI interface pointer), and the held interface is released first, so
//! the object need not already be empty; for a projected value type it is `winrt::impl::abi_t<T>*`. Paired with @ref get_abi.
//! @tparam T A C++/WinRT projected type.
//! @param object The object that receives the ABI value; for an interface or runtime class, any held interface is released first.
//! @return For an interface or runtime class, a `void**`; for a value type, a `winrt::impl::abi_t<T>*`.
template <typename T>
auto put_abi(T& object) noexcept
{
    return winrt::put_abi(object);
}

//! Returns the address of a `winrt::hstring`'s backing `HSTRING`, as an `HSTRING*`.
//! @param object The (empty) string that receives the `HSTRING`.
//! @return The address of `object`'s backing `HSTRING`, suitable as an out-parameter.
inline auto put_abi(winrt::hstring& object) noexcept
{
    return reinterpret_cast<HSTRING*>(winrt::put_abi(object));
}

//! Returns the raw `IUnknown` ABI pointer for a C++/WinRT object, without taking a reference.
//! @param ptr The object whose ABI pointer to return.
//! @return The `IUnknown*` backing `ptr` (borrowed, not AddRef'd; may be null).
inline ::IUnknown* com_raw_ptr(const winrt::Windows::Foundation::IUnknown& ptr) noexcept
{
    return static_cast<::IUnknown*>(winrt::get_abi(ptr));
}

//! Returns the raw `IInspectable` ABI pointer for a C++/WinRT object, without taking a reference.
//! This overload supplies the `IInspectable` that `wil::cx_object_from_abi` requires.
//! @param ptr The object whose ABI pointer to return.
//! @return The `IInspectable*` backing `ptr` (borrowed, not AddRef'd; may be null).
inline ::IInspectable* com_raw_ptr(const winrt::Windows::Foundation::IInspectable& ptr) noexcept
{
    return static_cast<::IInspectable*>(winrt::get_abi(ptr));
}

//! Constructs a C++/WinRT projected object from a raw ABI interface pointer by querying for the projected type.
//! Calls `QueryInterface` for `T`'s default interface and returns the resulting projection.
//! @tparam T The C++/WinRT projected type to construct.
//! @param from The interface pointer to convert from; must not be null.
//! @return A `T` referring to the same object as `from`.
//! @note Throws a `winrt::hresult_error` if the `QueryInterface` call fails.
template <typename T>
T convert_from_abi(::IUnknown* from)
{
    T result{nullptr}; // `T` is a projected type.
    winrt::check_hresult(from->QueryInterface(winrt::guid_of<T>(), winrt::put_abi(result)));
    return result;
}

/** Calls an interop method on a C++/WinRT type's activation factory and returns the object it produces.
Retrieves the activation factory for `WinRTFactory`, queries it for `Interface`, invokes `method` with `args`, and wraps the
result as a `WinRTResult`. Use this with factory interop interfaces such as `IInputPaneInterop`.
~~~
// Produce an object of the factory's own type:
winrt::InputPane inputPane = wil::capture_interop<winrt::InputPane>(&IInputPaneInterop::GetForWindow, hwnd);

// Produce a different type from the one whose factory exposes the interop method:
winrt::IAsyncAction action = wil::capture_interop<winrt::IAsyncAction, winrt::AccountsSettingsPane>(
    &IAccountsSettingsPaneInterop::ShowAddAccountForWindow, hwnd);
~~~
@tparam WinRTResult The C++/WinRT type to return.
@tparam WinRTFactory The C++/WinRT type whose activation factory exposes `method`; defaults to `WinRTResult`.
@param method Pointer to the interop interface method to call.
@param args Arguments to forward to `method`, ahead of the capture out-parameters it fills in.
@return The `WinRTResult` produced by `method`.
@note Throws a `winrt::hresult_error` if the factory lookup or the interop call fails. */
template <typename WinRTResult, typename WinRTFactory = WinRTResult, typename Interface, typename... InterfaceArgs, typename... Args>
auto capture_interop(HRESULT (__stdcall Interface::*method)(InterfaceArgs...), Args&&... args)
{
    auto interop = winrt::get_activation_factory<WinRTFactory, Interface>();
    return winrt::capture<WinRTResult>(interop, method, std::forward<Args>(args)...);
}

/** Calls an interop method on an existing C++/WinRT object and returns the object it produces.
Queries `obj` for `Interface`, invokes `method` with `args`, and wraps the result as a `WinRTResult`. Use this with per-instance
interop interfaces such as `IUserActivityInterop`.
~~~
winrt::UserActivitySession session = wil::capture_interop<winrt::UserActivitySession>(
    activity, &IUserActivityInterop::CreateSessionForWindow, hwnd);
~~~
@tparam WinRTResult The C++/WinRT type to return.
@param obj The object to query for `Interface` and call the interop method on.
@param method Pointer to the interop interface method to call.
@param args Arguments to forward to `method`, ahead of the capture out-parameters it fills in.
@return The `WinRTResult` produced by `method`.
@note Throws a `winrt::hresult_error` if the query or the interop call fails. */
template <typename WinRTResult, typename Interface, typename... InterfaceArgs, typename... Args>
auto capture_interop(winrt::Windows::Foundation::IUnknown const& obj, HRESULT (__stdcall Interface::*method)(InterfaceArgs...), Args&&... args)
{
    return winrt::capture<WinRTResult>(obj.as<Interface>(), method, std::forward<Args>(args)...);
}

/** Holds a reference to the host C++/WinRT module to prevent it from being unloaded.
Normally, this is done by being in an IAsyncOperation coroutine or by holding a strong
reference to a C++/WinRT object hosted in the same module, but if you have neither,
you will need to hold a reference explicitly. For the WRL equivalent, see wrl_module_reference.

This can be used as a base, which permits EBO:
@code
struct NonWinrtObject : wil::winrt_module_reference
{
    int value;
};

// DLL will not be unloaded as long as NonWinrtObject is still alive.
auto p = std::make_unique<NonWinrtObject>();
@endcode

Or it can be used as a member (with [[no_unique_address]] to avoid
occupying any memory):
@code
struct NonWinrtObject
{
    int value;

    [[no_unique_address]] wil::winrt_module_reference module_ref;
};

// DLL will not be unloaded as long as NonWinrtObject is still alive.
auto p = std::make_unique<NonWinrtObject>();
@endcode

If using it to prevent the host DLL from unloading while a thread
or threadpool work item is still running, create the object before
starting the thread, and pass it to the thread. This avoids a race
condition where the host DLL could get unloaded before the thread starts.
@code
std::thread([module_ref = wil::winrt_module_reference()]() { do_background_work(); });

// Don't do this (race condition)
std::thread([]() { wil::winrt_module_reference module_ref; do_background_work(); }); // WRONG
@endcode

Also useful in coroutines that neither capture DLL-hosted COM objects, nor are themselves
DLL-hosted COM objects. (If the coroutine returns IAsyncAction or captures a get_strong()
of its containing WinRT class, then the IAsyncAction or strong reference will itself keep
a strong reference to the host module.)
@code
winrt::fire_and_forget ContinueBackgroundWork()
{
    // prevent DLL from unloading while we are running on a background thread.
    // Do this before switching to the background thread.
    wil::winrt_module_reference module_ref;

    co_await winrt::resume_background();
    do_background_work();
};
@endcode
*/
struct [[nodiscard]] winrt_module_reference
{
    //! Takes a reference on the host C++/WinRT module, incrementing its lock count.
    winrt_module_reference()
    {
        ++winrt::get_module_lock();
    }

    //! Takes an additional reference on the host module; every live copy holds its own lock.
    winrt_module_reference(winrt_module_reference const&) : winrt_module_reference()
    {
    }

    //! Releases this reference, decrementing the host module's lock count.
    ~winrt_module_reference()
    {
        --winrt::get_module_lock();
    }
};

/** Implements a C++/WinRT class where some interfaces are conditionally supported.
@code
// Assume the existence of a class "Version2" which says whether
// the IMyThing2 interface should be supported.
struct Version2 { static bool IsEnabled(); };

// Declare implementation class which conditionally supports IMyThing2.
struct MyThing : wil::winrt_conditionally_implements<MyThingT<MyThing>,
                     Version2, IMyThing2>
{
    // implementation goes here
};

@endcode

If `Version2::IsEnabled()` returns `false`, then the `QueryInterface`
for `IMyThing2` will fail.

Any interface not listed as conditional is assumed to be enabled unconditionally.

You can add additional Version / Interface pairs to the template parameter list.
Interfaces may be conditionalized on at most one Version class. If you need a
complex conditional, create a new helper class.

@code
// Helper class for testing two Versions.
struct Version2_or_greater {
    static bool IsEnabled() { return Version2::IsEnabled() || Version3::IsEnabled(); }
};

// This implementation supports IMyThing2 if either Version2 or Version3 is enabled,
// and supports IMyThing3 only if Version3 is enabled.
struct MyThing : wil::winrt_conditionally_implements<MyThingT<MyThing>,
Version2_or_greater, IMyThing2, Version3, IMyThing3>
{
    // implementation goes here
};
@endcode
*/
template <typename Implements, typename... Rest>
struct winrt_conditionally_implements : Implements
{
    using Implements::Implements;

    //! Overrides C++/WinRT's interface lookup so conditionally-implemented interfaces are hidden when disabled.
    //! For each `Version`/`Interface` pair, a `QueryInterface` for that interface succeeds only when the paired
    //! `Version::IsEnabled()` returns `true`; interfaces not listed as conditional resolve normally.
    //! @param iid The interface being queried for.
    //! @return A pointer to the requested interface, or null if it is conditionally disabled or unsupported.
    void* find_interface(winrt::guid const& iid) const noexcept override
    {
        static_assert(sizeof...(Rest) % 2 == 0, "Extra template parameters should come in groups of two");
        if (is_enabled<0, std::tuple<Rest...>>(iid))
        {
            return Implements::find_interface(iid);
        }
        return nullptr;
    }

private:
    template <std::size_t index, typename Tuple>
    static bool is_enabled(winrt::guid const& iid)
    {
        if constexpr (index >= std::tuple_size_v<Tuple>)
        {
            return true;
        }
        else
        {
            check_no_duplicates<1, index + 1, Tuple>();
            return (iid == winrt::guid_of<std::tuple_element_t<index + 1, Tuple>>()) ? std::tuple_element_t<index, Tuple>::IsEnabled()
                                                                                     : is_enabled<index + 2, Tuple>(iid);
        }
    }

    template <std::size_t index, std::size_t upto, typename Tuple>
    static constexpr void check_no_duplicates()
    {
        if constexpr (index < upto)
        {
            static_assert(
                !std::is_same_v<std::tuple_element_t<index, Tuple>, std::tuple_element_t<upto, Tuple>>,
                "Duplicate interfaces found in winrt_conditionally_implements");
            check_no_duplicates<index + 2, upto, Tuple>();
        }
    }
};
} // namespace wil

#endif // __WIL_CPPWINRT_INCLUDED
