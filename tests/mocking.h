#pragma once

#include <detours/detours.h>
#include <synchapi.h>

#include <wil/resource.h>
#include <wil/result_macros.h>
#include <wil/wistd_functional.h>

namespace witest
{
namespace details
{
    __declspec(selectany) wil::srwlock s_detourLock;

    template <typename FuncT>
    inline HRESULT Register(FuncT** target, FuncT* detour) noexcept
    {
        RETURN_IF_WIN32_ERROR(::DetourTransactionBegin());
        auto abortOnFailure = wil::scope_exit([&] {
            LOG_IF_WIN32_ERROR(::DetourTransactionAbort());
        });
        RETURN_IF_WIN32_ERROR(::DetourUpdateThread(::GetCurrentThread()));
        RETURN_IF_WIN32_ERROR(::DetourAttach(reinterpret_cast<void**>(target), reinterpret_cast<void*>(detour)));
        RETURN_IF_WIN32_ERROR(::DetourTransactionCommit());
        abortOnFailure.release();
        return S_OK;
    }

    template <typename FuncT>
    inline HRESULT Unregister(FuncT** target, FuncT* detour) noexcept
    {
        RETURN_IF_WIN32_ERROR(::DetourTransactionBegin());
        auto abortOnFailure = wil::scope_exit([&] {
            LOG_IF_WIN32_ERROR(::DetourTransactionAbort());
        });
        RETURN_IF_WIN32_ERROR(::DetourUpdateThread(::GetCurrentThread()));
        RETURN_IF_WIN32_ERROR(::DetourDetach(reinterpret_cast<void**>(target), reinterpret_cast<void*>(detour)));
        RETURN_IF_WIN32_ERROR(::DetourTransactionCommit());
        abortOnFailure.release();
        return S_OK;
    }

    template <typename DerivedT, typename FuncT>
    struct detoured_thread_base;

    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_thread_base<DerivedT, ReturnT(__stdcall*)(ArgsT...)>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);

        static __stdcall ReturnT callback(ArgsT... args)
        {
            for (auto ptr = DerivedT::s_threadInstance; ptr; ptr = ptr->m_next)
            {
                if (!ptr->m_reentry)
                {
                    return ptr->invoke(wistd::forward<ArgsT>(args)...);
                }
            }

            // All registered functions have been called; forward to the implementation
            return DerivedT::s_target(wistd::forward<ArgsT>(args)...);
        }

        ReturnT invoke(ArgsT... args)
        {
            auto pThis = static_cast<DerivedT*>(this);
            WI_ASSERT(!pThis->m_reentry);
            pThis->m_reentry = true;
            auto resetOnExit = wil::scope_exit([&] {
                pThis->m_reentry = false; // No guarantee that 'ReturnT' is a movable type; NRVO is not guaranteed
            });
            return pThis->m_detour(wistd::forward<ArgsT>(args)...);
        }
    };

    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_thread_base<DerivedT, ReturnT(__stdcall*)(ArgsT...) noexcept>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);

        static __stdcall ReturnT callback(ArgsT... args) noexcept
        {
            for (auto ptr = DerivedT::s_threadInstance; ptr; ptr = ptr->m_next)
            {
                if (!ptr->m_reentry)
                {
                    return ptr->invoke(wistd::forward<ArgsT>(args)...);
                }
            }

            // All registered functions have been called; forward to the implementation
            return DerivedT::s_target(wistd::forward<ArgsT>(args)...);
        }

        ReturnT invoke(ArgsT... args) noexcept
        {
            auto pThis = static_cast<DerivedT*>(this);
            WI_ASSERT(!pThis->m_reentry);
            pThis->m_reentry = true;
            auto resetOnExit = wil::scope_exit([&] {
                pThis->m_reentry = false; // No guarantee that 'ReturnT' is a movable type; NRVO is not guaranteed
            });
            return pThis->m_detour(wistd::forward<ArgsT>(args)...);
        }
    };

#if _M_IX86
    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_thread_base<DerivedT, ReturnT(__cdecl*)(ArgsT...)>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);

        static __cdecl ReturnT callback(ArgsT... args)
        {
            for (auto ptr = DerivedT::s_threadInstance; ptr; ptr = ptr->m_next)
            {
                if (!ptr->m_reentry)
                {
                    return ptr->invoke(wistd::forward<ArgsT>(args)...);
                }
            }

            // All registered functions have been called; forward to the implementation
            return DerivedT::s_target(wistd::forward<ArgsT>(args)...);
        }

        ReturnT invoke(ArgsT... args)
        {
            auto pThis = static_cast<DerivedT*>(this);
            WI_ASSERT(!pThis->m_reentry);
            pThis->m_reentry = true;
            auto resetOnExit = wil::scope_exit([&] {
                pThis->m_reentry = false; // No guarantee that 'ReturnT' is a movable type; NRVO is not guaranteed
            });
            return pThis->m_detour(wistd::forward<ArgsT>(args)...);
        }
    };

    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_thread_base<DerivedT, ReturnT(__cdecl*)(ArgsT...) noexcept>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);

        static __cdecl ReturnT callback(ArgsT... args) noexcept
        {
            for (auto ptr = DerivedT::s_threadInstance; ptr; ptr = ptr->m_next)
            {
                if (!ptr->m_reentry)
                {
                    return ptr->invoke(wistd::forward<ArgsT>(args)...);
                }
            }

            // All registered functions have been called; forward to the implementation
            return DerivedT::s_target(wistd::forward<ArgsT>(args)...);
        }

        ReturnT invoke(ArgsT... args) noexcept
        {
            auto pThis = static_cast<DerivedT*>(this);
            WI_ASSERT(!pThis->m_reentry);
            pThis->m_reentry = true;
            auto resetOnExit = wil::scope_exit([&] {
                pThis->m_reentry = false; // No guarantee that 'ReturnT' is a movable type; NRVO is not guaranteed
            });
            return pThis->m_detour(wistd::forward<ArgsT>(args)...);
        }
    };
#endif
}

//! An RAII type that manages a thread-specific detouring of a specific function.
//! This type is used to register a specific lambda (or function pointer) as the detour for a specific function that only gets
//! invoked on the thread it was created on. This helps avoid issues such as the lambda being invoked on an unexpected thread or
//! having it get destroyed while being invoked on a different thread.
template <auto TargetFn>
struct detoured_thread_function : details::detoured_thread_base<detoured_thread_function<TargetFn>, decltype(TargetFn)>
{
private:
    using base = details::detoured_thread_base<detoured_thread_function, decltype(TargetFn)>;
    using return_type = typename base::return_type;
    using function_type = typename base::function_type;

    friend base;

public:
    detoured_thread_function() = default;

#ifdef WIL_ENABLE_EXCEPTIONS
    template <typename Func>
    detoured_thread_function(Func&& func) noexcept(noexcept(reset(wistd::forward<Func>(func))))
    {
        THROW_IF_FAILED(reset(wistd::forward<Func>(func)));
    }
#endif

    detoured_thread_function(const detoured_thread_function&) = delete;
    detoured_thread_function& operator=(const detoured_thread_function&) = delete;

    detoured_thread_function(detoured_thread_function&& other) noexcept
    {
        swap(other);
    }

    detoured_thread_function& operator=(detoured_thread_function&& other) noexcept
    {
        swap(other);
        return *this;
    }

    ~detoured_thread_function()
    {
        WI_VERIFY_SUCCEEDED(reset());
    }

    void swap(detoured_thread_function& other) noexcept
    {
        wistd::swap_wil(m_next, other.m_next);
        wistd::swap_wil(m_detour, other.m_detour);
        wistd::swap_wil(m_reentry, other.m_reentry);

        // We need to swap positions in the linked list
        detoured_thread_function** thisPos = nullptr;
        detoured_thread_function** otherPos = nullptr;
        for (auto ptr = &s_threadInstance; *ptr; ptr = &(*ptr)->next)
        {
            if (*ptr == this)
                thisPos = ptr;
            else if (*ptr == &other)
                otherPos = ptr;
        }

        if (thisPos)
            *thisPos = &other;
        if (otherPos)
            *otherPos = this;
    }

    HRESULT reset() noexcept
    {
        if (m_detour)
        {
            m_detour = {};

            detoured_thread_function** entryPtr = &s_threadInstance;
            while (*entryPtr && (*entryPtr != this))
                entryPtr = &(*entryPtr)->m_next;

            // Faling this check would imply likely imply that this object is being destroyed on the wrong thread. No matter the
            // reason, this should be considered a pretty fatal error
            FAIL_FAST_IF_NULL(*entryPtr);
            *entryPtr = m_next;

            {
                auto lock = details::s_detourLock.lock_exclusive();
                if (--s_refCount == 0)
                {
                    RETURN_IF_FAILED(details::Unregister(&s_target, &base::callback));
                }
            }
        }

        return S_OK;
    }

    template <typename Func>
    HRESULT reset(Func&& func) noexcept(
        (wistd::is_reference_v<Func> || wistd::is_const_v<Func>) ? wistd::is_nothrow_copy_assignable_v<wistd::remove_reference_t<Func>>
                                                                 : wistd::is_nothrow_move_assignable_v<Func>)
    {
        RETURN_IF_FAILED(reset());

        {
            auto lock = details::s_detourLock.lock_exclusive();
            if (s_refCount == 0)
            {
                RETURN_IF_FAILED(details::Register(&s_target, &base::callback));
            }
            ++s_refCount;
        }

        m_detour = wistd::forward<Func>(func);
        m_next = s_threadInstance;
        s_threadInstance = this;
        return S_OK;
    }

private:

    detoured_thread_function* m_next = nullptr; // Linked list of registrations; support detouring more than once on same thread
    wistd::function<function_type> m_detour;
    bool m_reentry = false; // Used to handle forwarding to impl or the detour

    static inline thread_local detoured_thread_function* s_threadInstance = nullptr;
    static inline auto s_target = TargetFn;
    static inline int s_refCount = 0; // Guarded by details::s_detourLock
};
} // namespace witest
