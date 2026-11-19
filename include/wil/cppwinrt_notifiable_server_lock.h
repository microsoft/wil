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
//! A server lock that runs a callback once all references are released.

#ifndef __WIL_CPPWINRT_NOTIFIABLE_MODULE_LOCK_INCLUDED
#define __WIL_CPPWINRT_NOTIFIABLE_MODULE_LOCK_INCLUDED

#ifdef WINRT_BASE_H
#error You must include this header before including any winrt header
#endif

#ifndef WINRT_CUSTOM_MODULE_LOCK
#error You must define 'WINRT_CUSTOM_MODULE_LOCK' at the project level
#endif

#include <combaseapi.h>
#include <cstdint>
#include <functional>

namespace wil
{
/** A C++/WinRT module lock that invokes a callback when the server's last object is released.
Replaces C++/WinRT's default module lock (via `WINRT_CUSTOM_MODULE_LOCK`) with one backed by `CoAddRefServerProcess` /
`CoReleaseServerProcess`, so out-of-process COM servers can be told when their final object goes away (typically to begin shutting
the server down). Register a callback with @ref set_notifier; it runs when the reference count drops to zero.

This header must be included before any C++/WinRT header, and the project must define `WINRT_CUSTOM_MODULE_LOCK`; C++/WinRT then
routes its module lock through `winrt::get_module_lock()`, which returns the @ref instance() singleton.
~~~
// With WINRT_CUSTOM_MODULE_LOCK defined project-wide (e.g. pass -DWINRT_CUSTOM_MODULE_LOCK to the compiler), include
// this header before any C++/WinRT header:
#include <wil/cppwinrt_notifiable_server_lock.h>
#include <winrt/Windows.Foundation.h>

// During server startup, ask to be notified when the last COM object is released:
wil::notifiable_server_lock::instance().set_notifier([] {
    // The server now has no live objects; begin shutting down
    // (for example, signal the event that main() is blocked on).
});
~~~
@note Access the singleton returned by @ref instance(); you cannot create instances of this type yourself. */
struct notifiable_server_lock
{
    //! Increments the server process reference count and returns the new count.
    //! Called by C++/WinRT when an object is created or a module reference is taken. Wraps `CoAddRefServerProcess`.
    //! @return The incremented server process reference count.
    uint32_t operator++() noexcept
    {
        return CoAddRefServerProcess();
    }

    //! Decrements the server process reference count and, when it reaches zero, invokes the registered notifier.
    //! Called by C++/WinRT when an object or module reference is released. Wraps `CoReleaseServerProcess`; if that returns zero
    //! and a notifier has been set with @ref set_notifier, the notifier runs before this returns.
    //! @return The decremented server process reference count; zero means the last reference was released.
    uint32_t operator--()
    {
        const auto ref = CoReleaseServerProcess();
        if (ref == 0 && notifier)
        {
            notifier();
        }

        return ref;
    }

    //! Sets the callback to run when the server process reference count reaches zero.
    //! The callback (invoked from `operator--()`) typically starts tearing the server down, for example by exiting the process or
    //! signaling a shutdown event.
    //! @tparam Func A callable invocable as `void()`.
    //! @param func The callback to invoke when the last reference is released.
    template <typename Func>
    void set_notifier(Func&& func)
    {
        notifier = std::forward<Func>(func);
    }

    //! Clears any registered notifier, so releasing the last reference no longer runs a callback.
    //! Pass `nullptr` to this overload to remove any previously registered callback.
    void set_notifier(std::nullptr_t) noexcept
    {
        notifier = nullptr;
    }

    //! Returns the process-wide singleton lock.
    //! This is the instance C++/WinRT uses through `winrt::get_module_lock()`; register your notification callback on it with
    //! @ref set_notifier.
    //! @return A reference to the singleton `notifiable_server_lock`.
    static notifiable_server_lock& instance() noexcept
    {
        static notifiable_server_lock lock;
        return lock;
    }

private:
    notifiable_server_lock() = default;

    std::function<void()> notifier;
};
} // namespace wil

namespace winrt
{
//! Returns the module lock C++/WinRT uses for this server: the @ref wil::notifiable_server_lock singleton.
//! C++/WinRT calls this (because the project defines `WINRT_CUSTOM_MODULE_LOCK`) to obtain the object that tracks live objects;
//! register a shutdown callback on it with @ref wil::notifiable_server_lock::set_notifier.
inline auto& get_module_lock() noexcept
{
    return wil::notifiable_server_lock::instance();
}
} // namespace winrt

#endif