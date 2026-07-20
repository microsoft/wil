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
//! Smart pointers and other thin usability pattern wrappers over COM patterns.
#ifndef __WIL_COM_APARTMENT_VARIABLE_INCLUDED
#define __WIL_COM_APARTMENT_VARIABLE_INCLUDED

#include <any>
#include <objidl.h>
#include <roapi.h>
#include <type_traits>
#include <unordered_map>
#include <winrt/Windows.Foundation.h>

#include "com.h"
#include "cppwinrt.h"
#include "result_macros.h"
#include "win32_helpers.h"

#ifndef WIL_ENABLE_EXCEPTIONS
#error This header requires exceptions
#endif

namespace wil
{
//! Determines whether apartment variables are supported in the current process context.
//! Prior to Windows build 22365 the APIs needed to create apartment variables (for example `RoGetApartmentIdentifier`) did not
//! work in unpackaged processes, so this returns `false` there.
//! @return `true` if apartment variables can be used in the current process, `false` otherwise.
inline bool are_apartment_variables_supported()
{
    unsigned long long apartmentId{};
    return RoGetApartmentIdentifier(&apartmentId) != HRESULT_FROM_WIN32(ERROR_API_UNAVAILABLE);
}

//! Unique RAII wrapper for an apartment-shutdown registration cookie; unregisters via `RoUnregisterForApartmentShutdown`.
//! ~~~
//! // Register an apartment-shutdown observer; the cookie unregisters automatically when this goes out of scope.
//! wil::unique_apartment_shutdown_registration registration;
//! unsigned long long apartmentId{};
//! THROW_IF_FAILED(RoRegisterForApartmentShutdown(observer.get(), &apartmentId, registration.put()));
//!
//! // Inside the observer's IApartmentShutdown::OnUninitialize callback, release the cookie (see the note below):
//! registration.release();
//! ~~~
//! @note COM implicitly runs down the registration when it invokes the shutdown handler and does not allow calling unregister
//!       from within that callback, so `release()` this in the handler to avoid a double-free of the cookie.
using unique_apartment_shutdown_registration =
    unique_any<APARTMENT_SHUTDOWN_REGISTRATION_COOKIE, decltype(&::RoUnregisterForApartmentShutdown), ::RoUnregisterForApartmentShutdown>;

//! Default implementation of the platform primitives used by @ref wil::apartment_variable.
//! Provides apartment identity, apartment-shutdown registration, and COM initialization.
struct apartment_variable_platform
{
    //! Gets the identifier of the current COM apartment. Fail-fasts on failure.
    //! @return The current apartment's identifier.
    static unsigned long long GetApartmentId()
    {
        unsigned long long apartmentId{};
        FAIL_FAST_IF_FAILED(RoGetApartmentIdentifier(&apartmentId));
        return apartmentId;
    }

    //! Registers an apartment-shutdown observer. Throws on failure.
    //! @param observer The observer to notify when the current apartment shuts down.
    //! @return A @ref shutdown_type RAII registration that unregisters the observer on destruction.
    static auto RegisterForApartmentShutdown(IApartmentShutdown* observer)
    {
        unsigned long long id{};
        shutdown_type cookie;
        THROW_IF_FAILED(RoRegisterForApartmentShutdown(observer, &id, cookie.put()));
        return cookie;
    }

    //! Unregisters a previously registered apartment-shutdown observer. Fail-fasts on failure.
    //! @param cookie The registration cookie returned by @ref RegisterForApartmentShutdown.
    static void UnRegisterForApartmentShutdown(APARTMENT_SHUTDOWN_REGISTRATION_COOKIE cookie)
    {
        FAIL_FAST_IF_FAILED(RoUnregisterForApartmentShutdown(cookie));
    }

    //! Initializes COM on the calling thread.
    //! @param coinitFlags `COINIT` flags to pass to `CoInitializeEx`; defaults to `COINIT_MULTITHREADED` (`0`).
    //! @return An RAII object that uninitializes COM on destruction.
    static auto CoInitializeEx(DWORD coinitFlags = 0 /*COINIT_MULTITHREADED*/)
    {
        return wil::CoInitializeEx(coinitFlags);
    }

    //! Test hook: the delay, in milliseconds, injected before asynchronous apartment rundown to exercise race conditions.
    //! This platform sets it to `INFINITE`, which disables the hook.
    inline static constexpr unsigned long AsyncRundownDelayForTestingRaces = INFINITE;

    //! The RAII registration type returned by @ref RegisterForApartmentShutdown; unregisters on destruction.
    using shutdown_type = wil::unique_apartment_shutdown_registration;
};

//! Controls what happens when destroying an @ref wil::apartment_variable would leak its backing storage.
enum class apartment_variable_leak_action
{
    fail_fast, //!< Fail-fast if the storage would be leaked. This is the default and catches misuse in DLLs.
    ignore     //!< Silently accept the leak; use this for apartment variables in `.exe`s, where the leak is expected.
};

//! "Pins" the current module in memory by incrementing its module reference count and intentionally leaking that reference.
//! Use this to keep a DLL loaded when it hosts apartment variables whose backing storage may otherwise outlive the module (see
//! @ref wil::apartment_variable and its fail-fast notes).
inline void ensure_module_stays_loaded()
{
    static INIT_ONCE s_initLeakModule{}; // avoiding magic statics
    wil::init_once_failfast(s_initLeakModule, []() {
        HMODULE result{};
        FAIL_FAST_IF(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, L"", &result));
        return S_OK;
    });
}

/// @cond
namespace details
{
    // For the address of data, you can detect global variables by the ability to resolve the module from the address.
    inline bool IsGlobalVariable(const void* moduleAddress) noexcept
    {
        wil::unique_hmodule moduleHandle;
        return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, static_cast<PCWSTR>(moduleAddress), &moduleHandle) != FALSE;
    }

    struct any_maker_base
    {
        std::any (*adapter)(void*);
        void* inner;

        WI_NODISCARD std::any operator()() const
        {
            return adapter(inner);
        }
    };

    template <typename T>
    struct any_maker : any_maker_base
    {
        any_maker()
        {
            adapter = [](auto) -> std::any {
                return T{};
            };
        }

        any_maker(T (*maker)())
        {
            adapter = [](auto maker) -> std::any {
                return reinterpret_cast<T (*)()>(maker)();
            };
            inner = reinterpret_cast<void*>(maker);
        }

        template <typename F>
        any_maker(F&& f)
        {
            adapter = [](auto maker) -> std::any {
                return reinterpret_cast<F*>(maker)[0]();
            };
            inner = std::addressof(f);
        }
    };

    template <apartment_variable_leak_action leak_action = apartment_variable_leak_action::fail_fast, typename test_hook = apartment_variable_platform>
    struct apartment_variable_base
    {
    protected:
        inline static winrt::slim_mutex s_lock;

        struct apartment_variable_storage
        {
            apartment_variable_storage(apartment_variable_storage&& other) noexcept = default;
            apartment_variable_storage(const apartment_variable_storage& other) = delete;

            apartment_variable_storage(typename test_hook::shutdown_type&& cookie_) : cookie(std::move(cookie_))
            {
            }

            winrt::apartment_context context;
            typename test_hook::shutdown_type cookie;
            // Variables are stored using the address of the apartment_variable_base<> as the key.
            std::unordered_map<apartment_variable_base<leak_action, test_hook>*, std::any> variables;
        };

        // Apartment id -> variables storage.
        inline static wil::object_without_destructor_on_shutdown<std::unordered_map<unsigned long long, apartment_variable_storage>> s_apartmentStorage;

        // NOTE: Requires 's_lock' to be held
        static apartment_variable_storage* get_current_apartment_variable_storage()
        {
            auto storage = s_apartmentStorage.get().find(test_hook::GetApartmentId());
            if (storage != s_apartmentStorage.get().end())
            {
                return &storage->second;
            }
            return nullptr;
        }

        // NOTE: Requires 's_lock' to be held
        apartment_variable_storage* ensure_current_apartment_variables()
        {
            auto variables = get_current_apartment_variable_storage();
            if (variables)
            {
                return variables;
            }

            struct ApartmentObserver : public winrt::implements<ApartmentObserver, IApartmentShutdown>
            {
                void STDMETHODCALLTYPE OnUninitialize(unsigned long long apartmentId) noexcept override
                {
                    // This code runs at apartment rundown so be careful to avoid deadlocks by
                    // extracting the variables under the lock then release them outside.
                    auto variables = [apartmentId]() {
                        auto lock = winrt::slim_lock_guard(s_lock);
                        return s_apartmentStorage.get().extract(apartmentId);
                    }();
                    WI_ASSERT(variables.key() == apartmentId);
                    // The system implicitly releases the shutdown observer
                    // after invoking the callback and does not allow calling unregister
                    // in the callback. So release the reference to the registration.
                    variables.mapped().cookie.release();
                }
            };
            auto shutdownRegistration = test_hook::RegisterForApartmentShutdown(winrt::make<ApartmentObserver>().get());
            return &s_apartmentStorage.get()
                        .insert({test_hook::GetApartmentId(), apartment_variable_storage(std::move(shutdownRegistration))})
                        .first->second;
        }

    public:
        constexpr apartment_variable_base() = default;
        ~apartment_variable_base()
        {
            // Global variables (object with static storage duration)
            // are run down when the process is shutting down or when the
            // dll is unloaded. At these points it is not possible to start
            // an async operation and the work performed is not needed,
            // the apartments with variable have been run down already.
            const auto isGlobal = details::IsGlobalVariable(this);
            if (!isGlobal)
            {
                clear_all_apartments_async();
            }

            if constexpr (leak_action == apartment_variable_leak_action::fail_fast)
            {
                if (isGlobal && !ProcessShutdownInProgress())
                {
                    // If you hit this fail fast it means the storage in s_apartmentStorage will be leaked.
                    // For apartment variables used in .exes, this is expected and
                    // this fail fast should be disabled using
                    // wil::apartment_variable<T, wil::apartment_variable_leak_action::ignore>
                    //
                    // For DLLs, if this is expected, disable this fail fast using
                    // wil::apartment_variable<T, wil::apartment_variable_leak_action::ignore>
                    //
                    // Use of apartment variables in DLLs only loaded by COM will never hit this case
                    // as COM will unload DLLs before apartments are rundown,
                    // providing the opportunity to empty s_apartmentStorage.
                    //
                    // But DLLs loaded and unloaded to call DLL entry points (outside of COM) may
                    // create variable storage that can't be cleaned up as the DLL lifetime is
                    // shorter that the COM lifetime. In these cases either
                    // 1) accept the leaks and disable the fail fast as describe above
                    // 2) disable module unloading by calling wil::ensure_module_stays_loaded
                    // 3) CoCreate an object from this DLL to make COM aware of the DLL
                    FAIL_FAST_IF(!s_apartmentStorage.get().empty());
                }
            }
        }

        // non-copyable, non-assignable
        apartment_variable_base(apartment_variable_base const&) = delete;
        void operator=(apartment_variable_base const&) = delete;

        // get current value or throw if no value has been set
        std::any& get_existing()
        {
            auto any = get_if();
            if (!any)
            {
                THROW_HR(E_NOT_SET);
            }

            return *any;
        }

        // get current value or custom-construct one on demand
        template <typename T>
        std::any& get_or_create(any_maker<T>&& creator)
        {
            apartment_variable_storage* variable_storage = nullptr;

            { // scope for lock
                auto lock = winrt::slim_lock_guard(s_lock);
                variable_storage = ensure_current_apartment_variables();

                auto variable = variable_storage->variables.find(this);
                if (variable != variable_storage->variables.end())
                {
                    return variable->second;
                }
            } // drop the lock

            // create the object outside the lock to avoid reentrant deadlock
            auto value = creator();

            auto insert_lock = winrt::slim_lock_guard(s_lock);
            // The insertion may fail if creator() recursively caused itself to be created,
            // in which case we return the existing object and the falsely-created one is discarded.
            return variable_storage->variables.insert({this, std::move(value)}).first->second;
        }

        // get pointer to current value or nullptr if no value has been set
        std::any* get_if()
        {
            auto lock = winrt::slim_lock_guard(s_lock);

            if (auto variable_storage = get_current_apartment_variable_storage())
            {
                auto variable = variable_storage->variables.find(this);
                if (variable != variable_storage->variables.end())
                {
                    return &(variable->second);
                }
            }
            return nullptr;
        }

        // replace or create the current value, fail fasts if the value is not already stored
        void set(std::any value)
        {
            // release value, with the swapped value, outside of the lock
            {
                auto lock = winrt::slim_lock_guard(s_lock);
                auto storage = s_apartmentStorage.get().find(test_hook::GetApartmentId());
                FAIL_FAST_IF(storage == s_apartmentStorage.get().end());
                auto& variable_storage = storage->second;
                auto variable = variable_storage.variables.find(this);
                FAIL_FAST_IF(variable == variable_storage.variables.end());
                variable->second.swap(value);
            }
        }

        // remove any current value
        void clear()
        {
            auto lock = winrt::slim_lock_guard(s_lock);
            if (auto variable_storage = get_current_apartment_variable_storage())
            {
                variable_storage->variables.erase(this);
                if (variable_storage->variables.size() == 0)
                {
                    s_apartmentStorage.get().erase(test_hook::GetApartmentId());
                }
            }
        }

        winrt::Windows::Foundation::IAsyncAction clear_all_apartments_async()
        {
            // gather all the apartments that hold objects we need to destruct
            // (do not gather the objects themselves, because the apartment might
            // destruct before we get around to it, and we should let the apartment
            // destruct the object while it still can).

            std::vector<winrt::apartment_context> contexts;
            { // scope for lock
                auto lock = winrt::slim_lock_guard(s_lock);
                for (auto& [id, storage] : s_apartmentStorage.get())
                {
                    auto variable = storage.variables.find(this);
                    if (variable != storage.variables.end())
                    {
                        contexts.push_back(storage.context);
                    }
                }
            }

            if (contexts.empty())
            {
                co_return;
            }

            wil::unique_mta_usage_cookie mta_reference; // need to extend the MTA due to async cleanup
            FAIL_FAST_IF_FAILED(CoIncrementMTAUsage(mta_reference.put()));

            // From a background thread hop into each apartment to run down the object
            // if it's still there.
            co_await winrt::resume_background();

            // This hook enables testing the case where execution of this method loses the race with
            // apartment rundown by other means.
            if constexpr (test_hook::AsyncRundownDelayForTestingRaces != INFINITE)
            {
                Sleep(test_hook::AsyncRundownDelayForTestingRaces);
            }

            for (auto&& context : contexts)
            {
                try
                {
                    co_await context;
                    clear();
                }
                catch (winrt::hresult_error const& e)
                {
                    // Ignore failure if apartment ran down before we could clean it up.
                    // The object already ran down as part of apartment cleanup.
                    if ((e.code() != RPC_E_SERVER_DIED_DNE) && (e.code() != RPC_E_DISCONNECTED))
                    {
                        throw;
                    }
                }
                catch (...)
                {
                    FAIL_FAST();
                }
            }
        }

        static const auto& storage()
        {
            return s_apartmentStorage.get();
        }

        static size_t current_apartment_variable_count()
        {
            auto lock = winrt::slim_lock_guard(s_lock);
            if (auto variable_storage = get_current_apartment_variable_storage())
            {
                return variable_storage->variables.size();
            }
            return 0;
        }
    };
} // namespace details
/// @endcond

/** A variable whose value is stored per COM apartment, so COM objects can be held safely in globals.
Apartment variables store a unique copy of `T` in each apartment and manage its lifetime based on apartment-rundown notifications,
which lets you keep COM objects in globals (objects with static storage duration) safely. They can also be used for automatic or
dynamic storage duration, but those cases are less common. This type is also useful for storing references to apartment-affine
objects.

~~~
// A per-apartment cache stored safely in a global.
wil::apartment_variable<winrt::com_ptr<IMyService>> g_service;

winrt::com_ptr<IMyService> GetService()
{
    // Constructed once per apartment on first use; destroyed when the apartment runs down.
    return g_service.get_or_create([] { return CreateServiceForThisApartment(); });
}
~~~

Apartment variables hosted in a COM DLL need to integrate with the DLL's `DllCanUnloadNow()` so that the reference counts
contributed by C++/WinRT objects are included. This is automatic for DLLs that host C++/WinRT objects, but WRL projects need to be
updated to call `winrt::get_module_lock()`:
~~~
// Example implementation of a WRL DLL's DllCanUnloadNow
HRESULT __stdcall DllCanUnloadNow()
{
    // Keep the DLL loaded while WRL objects OR C++/WinRT objects (including apartment variables) are outstanding.
    const bool wrlInUse = Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().GetObjectCount() > 0;
    return (wrlInUse || winrt::get_module_lock()) ? S_FALSE : S_OK;
}
~~~
@tparam T The type stored, one copy per apartment.
@tparam leak_action Whether to fail-fast (the default) or ignore if the backing storage would be leaked when the variable is
        destroyed. See @ref apartment_variable_leak_action.
@tparam test_hook Internal testing seam for the platform primitives; defaults to @ref apartment_variable_platform and should be
        left unchanged for normal consumers. */
template <typename T, apartment_variable_leak_action leak_action = apartment_variable_leak_action::fail_fast, typename test_hook = wil::apartment_variable_platform>
struct apartment_variable : details::apartment_variable_base<leak_action, test_hook>
{
    /// @cond
    using base = details::apartment_variable_base<leak_action, test_hook>;
    /// @endcond

    constexpr apartment_variable() = default;

    //! Gets the current apartment's value, throwing a WIL exception (`E_NOT_SET`) if no value has been set.
    //! @return A reference to the current apartment's value.
    T& get_existing()
    {
        return std::any_cast<T&>(base::get_existing());
    }

    //! Gets the current apartment's value, default-constructing it on first use.
    //! @return A reference to the current apartment's value.
    T& get_or_create()
    {
        return std::any_cast<T&>(base::get_or_create(details::any_maker<T>()));
    }

    //! Gets the current apartment's value, constructing it with `f` on first use.
    //! @tparam F A callable returning a `T` (or something convertible to it).
    //! @param f Invoked to construct the value the first time it is needed in the current apartment.
    //! @return A reference to the current apartment's value.
    template <typename F>
    T& get_or_create(F&& f)
    {
        return std::any_cast<T&>(base::get_or_create(details::any_maker<T>(std::forward<F>(f))));
    }

    //! Gets a pointer to the current apartment's value.
    //! @return A pointer to the current apartment's value, or `nullptr` if no value has been set.
    T* get_if()
    {
        return std::any_cast<T>(base::get_if());
    }

    //! Replaces the current apartment's value; fail-fasts if no value is currently stored.
    //! @tparam V The type of the value to store (convertible to `T`).
    //! @param value The new value to store for the current apartment.
    template <typename V>
    void set(V&& value)
    {
        return base::set(std::forward<V>(value));
    }

    //! Clears (destroys) the value in the current apartment.
    using base::clear;

    //! Asynchronously clears the value in every apartment in which it is present.
    using base::clear_all_apartments_async;

    /// @cond
    // For testing only.
    // 1) To observe the state of the storage in the debugger assign this to
    // a temporary variable (const&) and watch its contents.
    // 2) Use this to test the implementation.
    using base::storage;
    // For testing only. The number of variables in the current apartment.
    using base::current_apartment_variable_count;
    /// @endcond
};
} // namespace wil

#endif // __WIL_COM_APARTMENT_VARIABLE_INCLUDED
