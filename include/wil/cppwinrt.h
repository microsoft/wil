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
    // For example: "2.0.210122.3"
    inline constexpr int version_from_string(const char* versionString)
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

    inline constexpr int major_version_from_string(const char* versionString)
    {
        return version_from_string(versionString);
    }

    inline constexpr int minor_version_from_string(const char* versionString)
    {
        auto str = versionString;
        int dotCount = 0;
        while ((*str != '\0'))
        {
            if (*str == '.')
            {
                ++dotCount;
            }

            ++str;
            if (dotCount == 2)
            {
                break;
            }
        }

        if (*str == '\0')
        {
            return 0;
        }

        return version_from_string(str);
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

// The same is true with this function pointer as well, except that the version must be 2.X or higher.
extern void(__stdcall* winrt_throw_hresult_handler)(uint32_t, char const*, char const*, void*, winrt::hresult const) noexcept;

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
}
/// @endcond

namespace wil
{
    inline std::int32_t __stdcall winrt_to_hresult(void* returnAddress) noexcept
    {
        // C++/WinRT only gives us the return address (caller), so pass along an empty 'DiagnosticsInfo' since we don't
        // have accurate file/line/etc. information
        return static_cast<std::int32_t>(details::ReportFailure_CaughtException<FailureType::Return>(__R_DIAGNOSTICS_RA(DiagnosticsInfo{}, returnAddress)));
    }

    inline void __stdcall winrt_throw_hresult(uint32_t lineNumber, char const* fileName, char const* functionName, void* returnAddress, winrt::hresult const result) noexcept
    {
        void* callerReturnAddress{nullptr}; PCSTR code{nullptr};
        wil::details::ReportFailure_Hr<FailureType::Log>(__R_FN_CALL_FULL __R_COMMA result);
    }

    inline void WilInitialize_CppWinRT()
    {
        details::g_pfnResultFromCaughtException_CppWinRt = details::ResultFromCaughtException_CppWinRt;
        if constexpr (details::major_version_from_string(CPPWINRT_VERSION) >= 2)
        {
            WI_ASSERT(winrt_to_hresult_handler == nullptr);
            winrt_to_hresult_handler = winrt_to_hresult;
        }

        if constexpr ((details::major_version_from_string(CPPWINRT_VERSION) >= 2) && (details::minor_version_from_string(CPPWINRT_VERSION) >= 210122))
        {
            WI_ASSERT(winrt_throw_hresult_handler == nullptr);
            winrt_throw_hresult_handler = winrt_throw_hresult;
        }
    }

    /// @cond
    namespace details
    {
#ifndef CPPWINRT_SUPPRESS_STATIC_INITIALIZERS
        WI_ODR_PRAGMA("CPPWINRT_SUPPRESS_STATIC_INITIALIZERS", "0")
        WI_HEADER_INITITALIZATION_FUNCTION(WilInitialize_CppWinRT, []
        {
            ::wil::WilInitialize_CppWinRT();
            return 1;
        });
#else
        WI_ODR_PRAGMA("CPPWINRT_SUPPRESS_STATIC_INITIALIZERS", "1")
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

    inline ::IUnknown* com_raw_ptr(const winrt::Windows::Foundation::IUnknown& ptr) noexcept
    {
        return static_cast<::IUnknown*>(winrt::get_abi(ptr));
    }

    // Needed to power wil::cx_object_from_abi that requires IInspectable
    inline ::IInspectable* com_raw_ptr(const winrt::Windows::Foundation::IInspectable& ptr) noexcept
    {
        return static_cast<::IInspectable*>(winrt::get_abi(ptr));
    }

    // Taken from the docs.microsoft.com article
    template <typename T>
    T convert_from_abi(::IUnknown* from)
    {
        T to{ nullptr }; // `T` is a projected type.
        winrt::check_hresult(from->QueryInterface(winrt::guid_of<T>(), winrt::put_abi(to)));
        return to;
    }

    // For obtaining an object from an interop method on the factory. Example:
    // winrt::InputPane inputPane = wil::capture_interop<winrt::InputPane>(&IInputPaneInterop::GetForWindow, hwnd);
    // If the method produces something different from the factory type:
    // winrt::IAsyncAction action = wil::capture_interop<winrt::IAsyncAction, winrt::AccountsSettingsPane>(&IAccountsSettingsPaneInterop::ShowAddAccountForWindow, hwnd);
    template<typename WinRTResult, typename WinRTFactory = WinRTResult, typename Interface, typename... InterfaceArgs, typename... Args>
    auto capture_interop(HRESULT(__stdcall Interface::* method)(InterfaceArgs...), Args&&... args)
    {
        auto interop = winrt::get_activation_factory<WinRTFactory, Interface>();
        return winrt::capture<WinRTResult>(interop, method, std::forward<Args>(args)...);
    }

    // For obtaining an object from an interop method on an instance. Example:
    // winrt::UserActivitySession session = wil::capture_interop<winrt::UserActivitySession>(activity, &IUserActivityInterop::CreateSessionForWindow, hwnd);
    template<typename WinRTResult, typename Interface, typename... InterfaceArgs, typename... Args>
    auto capture_interop(winrt::Windows::Foundation::IUnknown const& o, HRESULT(__stdcall Interface::* method)(InterfaceArgs...), Args&&... args)
    {
        return winrt::capture<WinRTResult>(o.as<Interface>(), method, std::forward<Args>(args)...);
    }
}

#if (defined(__cpp_lib_coroutine) && (__cpp_lib_coroutine >= 201902L)) || defined(_RESUMABLE_FUNCTIONS_SUPPORTED)

/// @cond
#if defined(_RESUMABLE_FUNCTIONS_SUPPORTED)
#include <experimental/coroutine>
namespace wil::details
{
    template<typename T = void> using coroutine_handle = std::experimental::coroutine_handle<T>;
}
#else
#include <coroutine>
namespace wil::details
{
    template<typename T = void> using coroutine_handle = std::coroutine_handle<T>;
}
#endif

namespace winrt::Windows::UI::Core
{
    struct CoreDispatcher;
    enum class CoreDispatcherPriority;
    struct DispatchedHandler;
}

namespace winrt::Windows::System
{
    struct DispatcherQueue;
    enum class DispatcherQueuePriority;
    struct DispatcherQueueHandler;
}

namespace winrt::Microsoft::System
{
    struct DispatcherQueue;
    enum class DispatcherQueuePriority;
    struct DispatcherQueueHandler;
}

namespace winrt::Microsoft::UI::Dispatching
{
    struct DispatcherQueue;
    enum class DispatcherQueuePriority;
    struct DispatcherQueueHandler;
}
/// @endcond

namespace wil
{
    /// @cond
    namespace details
    {
        struct dispatcher_RunAsync
        {
            template<typename Dispatcher, typename... Args>
            static void Schedule(Dispatcher const& dispatcher, Args&&... args)
            {
                dispatcher.RunAsync(std::forward<Args>(args)...);
            }
        };

        struct dispatcher_TryEnqueue
        {
            template<typename Dispatcher,typename... Args>
            static void Schedule(Dispatcher const& dispatcher, Args&&... args)
            {
                dispatcher.TryEnqueue(std::forward<Args>(args)...);
            }
        };

        template<typename Dispatcher> struct dispatcher_traits;

        template<>
        struct dispatcher_traits<winrt::Windows::UI::Core::CoreDispatcher>
        {
            using Priority = winrt::Windows::UI::Core::CoreDispatcherPriority;
            using Handler = winrt::Windows::UI::Core::DispatchedHandler;
            using Scheduler = dispatcher_RunAsync;
        };

        template<>
        struct dispatcher_traits<winrt::Windows::System::DispatcherQueue>
        {
            using Priority = winrt::Windows::System::DispatcherQueuePriority;
            using Handler = winrt::Windows::System::DispatcherQueueHandler;
            using Scheduler = dispatcher_TryEnqueue;
        };

        template<>
        struct dispatcher_traits<winrt::Microsoft::System::DispatcherQueue>
        {
            using Priority = winrt::Microsoft::System::DispatcherQueuePriority;
            using Handler = winrt::Microsoft::System::DispatcherQueueHandler;
            using Scheduler = dispatcher_TryEnqueue;
        };

        template<>
        struct dispatcher_traits<winrt::Microsoft::UI::Dispatching::DispatcherQueue>
        {
            using Priority = winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority;
            using Handler = winrt::Microsoft::UI::Dispatching::DispatcherQueueHandler;
            using Scheduler = dispatcher_TryEnqueue;
        };

        struct dispatched_handler_state
        {
            details::coroutine_handle<> handle;
            bool orphaned = false;
        };

        struct dispatcher_handler
        {
            dispatcher_handler(dispatched_handler_state* state) : m_state(state) { }
            dispatcher_handler(dispatcher_handler&& other) noexcept : m_state(std::exchange(other.m_state, {})) {}

            ~dispatcher_handler()
            {
                if (m_state && m_state->handle)
                {
                    m_state->orphaned = true;
                    Complete();
                }
            }
            void operator()()
            {
                Complete();
            }

            void Complete()
            {
                auto state = std::exchange(m_state, nullptr);
                std::exchange(state->handle, {}).resume();
            }

            dispatched_handler_state* m_state;
        };
    }
    /// @endcond

    //! Resumes coroutine execution on the thread associated with the dispatcher, or throws
    //! an exception (from an arbitrary thread) if unable. Supported dispatchers are
    //! Windows.System.DispatcherQueue, Microsoft.System.DispatcherQueue,
    //! Microsoft.UI.Dispatching.DispatcherQueue, and Windows.UI.Core.CoreDispatcher.
    template<typename Dispatcher>
    [[nodiscard]] auto resume_foreground(Dispatcher const& dispatcher,
        typename details::dispatcher_traits<Dispatcher>::Priority priority = details::dispatcher_traits<Dispatcher>::Priority::Normal)
    {
        using Traits = details::dispatcher_traits<Dispatcher>;
        using Priority = typename Traits::Priority;
        using Handler = typename Traits::Handler;

        struct awaitable
        {
            awaitable(Dispatcher const& dispatcher, Priority priority) noexcept :
                m_dispatcher(dispatcher),
                m_priority(priority)
            {
            }
            bool await_ready() const noexcept { return false; }

            void await_suspend(details::coroutine_handle<> handle)
            {
                m_state.handle = handle;
                Handler handler{ details::dispatcher_handler(&m_state) };
                try
                {
                    // The return value of Schedule is not reliable. Use the dispatcher_handler destructor
                    // to detect whether the work item failed to run.
                    Traits::Scheduler::Schedule(m_dispatcher, m_priority, handler);
                }
                catch (...)
                {
                    m_state.handle = nullptr; // the exception will resume the coroutine, so the handler shouldn't do it
                    throw;
                }
            }

            void await_resume() const
            {
                if (m_state.orphaned)
                {
                    throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_NO_TASK_QUEUE));
                }
            }

        private:
            Dispatcher const& m_dispatcher;
            Priority const m_priority;
            details::dispatched_handler_state m_state;
        };
        return awaitable{ dispatcher, priority };
    }
}
#endif // C++20 coroutines supported

#endif // __WIL_CPPWINRT_INCLUDED
