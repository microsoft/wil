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
struct notifiable_server_lock
{
    uint32_t operator++() noexcept
    {
        return CoAddRefServerProcess();
    }

    uint32_t operator--()
    {
        const auto ref = CoReleaseServerProcess();
        if (ref == 0 && notifier)
        {
            notifier();
        }

        return ref;
    }

    template <typename Func>
    void set_notifier(Func&& func)
    {
        notifier = std::forward<Func>(func);
    }

    void set_notifier(std::nullptr_t) noexcept
    {
        notifier = nullptr;
    }

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
inline auto& get_module_lock() noexcept
{
    return wil::notifiable_server_lock::instance();
}
} // namespace winrt

#endif