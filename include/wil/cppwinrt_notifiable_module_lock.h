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
//! Utilities for implementing OOP COM server with cppwinrt only

#ifndef __WIL_CPPWINRT_NOTIFIABLE_MODULE_LOCK_DEFINED
#define __WIL_CPPWINRT_NOTIFIABLE_MODULE_LOCK_DEFINED

#if _MSVC_LANG >= 201703L

#include <atomic>
#include <cstdint>

// Ligh up cppwinrt custom module lock
#define WINRT_CUSTOM_MODULE_LOCK

namespace wil
{
    // Adopted from cppwinrt
    template <typename Func>
    struct notifiable_module_lock
    {
        notifiable_module_lock() = default;

        notifiable_module_lock(uint32_t count) : m_count(count)
        {
        }

        uint32_t operator=(uint32_t count) noexcept
        {
            return m_count = count;
        }

        uint32_t operator++() noexcept
        {
            return m_count.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        uint32_t operator--() noexcept
        {
            auto const remaining = m_count.fetch_sub(1, std::memory_order_release) - 1;

            if (remaining == 0)
            {
                std::atomic_thread_fence(std::memory_order_acquire);
                notifier();
            }
            else if (remaining < 0)
            {
                abort();
            }

            return remaining;
        }

        operator uint32_t() const noexcept
        {
            return m_count;
        }

        static void set_notifier(Func& func)
        {
            notifier = func;
        }

    private:
        std::atomic<int32_t> m_count;
        static inline Func notifier;
    };
}

#ifndef WIL_CPPWINRT_COM_SERVER_CUSTOM_MODULE_LOCK

namespace wil
{
    void set_cppwinrt_module_notifier(void(*func)())
    {
        wil::notifiable_module_lock<void (*)()>::set_notifier(func);
    }
}

namespace winrt
{
    auto& get_module_lock()
    {
        static wil::notifiable_module_lock<void(*)()> lock;
        return lock;
    }
}

#endif

#endif

#endif