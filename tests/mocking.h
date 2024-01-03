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
    struct detoured_global_function_base;

    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_global_function_base<DerivedT, ReturnT(__stdcall*)(ArgsT...)>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);
        static constexpr bool is_noexcept = false;

        static ReturnT __stdcall callback(ArgsT... args)
        {
            return DerivedT::invoke_callback(wistd::forward<ArgsT>(args)...);
        }
    };

    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_global_function_base<DerivedT, ReturnT(__stdcall*)(ArgsT...) noexcept>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);
        static constexpr bool is_noexcept = true;

        static ReturnT __stdcall callback(ArgsT... args) noexcept
        {
            return DerivedT::invoke_callback(wistd::forward<ArgsT>(args)...);
        }
    };

#if _M_IX86
    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_global_function_base<DerivedT, ReturnT(__cdecl*)(ArgsT...)>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);
        static constexpr bool is_noexcept = false;

        static ReturnT __cdecl callback(ArgsT... args)
        {
            return DerivedT::invoke_callback(wistd::forward<ArgsT>(args)...);
        }
    };

    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_global_function_base<DerivedT, ReturnT(__cdecl*)(ArgsT...) noexcept>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);
        static constexpr bool is_noexcept = true;

        static ReturnT __cdecl callback(ArgsT... args) noexcept
        {
            return DerivedT::invoke_callback(wistd::forward<ArgsT>(args)...);
        }
    };
#endif

    template <typename DerivedT, typename FuncT>
    struct detoured_thread_function_base;

    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_thread_function_base<DerivedT, ReturnT(__stdcall*)(ArgsT...)>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);
        static constexpr bool is_noexcept = false;

        static ReturnT __stdcall callback(ArgsT... args)
        {
            return DerivedT::invoke_callback(wistd::forward<ArgsT>(args)...);
        }
    };

    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_thread_function_base<DerivedT, ReturnT(__stdcall*)(ArgsT...) noexcept>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);
        static constexpr bool is_noexcept = true;

        static ReturnT __stdcall callback(ArgsT... args) noexcept
        {
            return DerivedT::invoke_callback(wistd::forward<ArgsT>(args)...);
        }
    };

#if _M_IX86
    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_thread_function_base<DerivedT, ReturnT(__cdecl*)(ArgsT...)>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);
        static constexpr bool is_noexcept = false;

        static ReturnT __cdecl callback(ArgsT... args)
        {
            return DerivedT::invoke_callback(wistd::forward<ArgsT>(args)...);
        }
    };

    template <typename DerivedT, typename ReturnT, typename... ArgsT>
    struct detoured_thread_function_base<DerivedT, ReturnT(__cdecl*)(ArgsT...) noexcept>
    {
    protected:
        using return_type = ReturnT;
        using function_type = ReturnT(ArgsT...);
        static constexpr bool is_noexcept = true;

        static ReturnT __cdecl callback(ArgsT... args) noexcept
        {
            return DerivedT::invoke_callback(wistd::forward<ArgsT>(args)...);
        }
    };
#endif
} // namespace details

//! An RAII type that manages the global detouring of a specific function.
//! This type is used to register a specific lambda (or function pointer) as the detour for a specific function that gets invoked
//! on all threads, not just the one that this object was created on.
template <auto TargetFn>
struct detoured_global_function : details::detoured_global_function_base<detoured_global_function<TargetFn>, decltype(TargetFn)>
{
private:
    using base = details::detoured_global_function_base<detoured_global_function, decltype(TargetFn)>;
    using return_type = typename base::return_type;
    using function_type = typename base::function_type;

    friend base;

public:
    detoured_global_function() = default;

#ifdef WIL_ENABLE_EXCEPTIONS
    template <typename Func>
    detoured_global_function(Func&& func) noexcept(noexcept(reset(wistd::forward<Func>(func))))
    {
        THROW_IF_FAILED(reset(wistd::forward<Func>(func)));
    }
#endif

    detoured_global_function(const detoured_global_function&) = delete;
    detoured_global_function& operator=(const detoured_global_function&) = delete;
    // It's not safe to move construct/assign this type because it's not safe to move the function object while it's potentially
    // being invoked by a different thread

    ~detoured_global_function()
    {
        WI_VERIFY_SUCCEEDED(reset());
    }

    HRESULT reset() noexcept
    {
        if (m_detour)
        {
            // The detour can be invoked from any thread, so we need to wait for any thread concurrently invoking the detour to
            // complete before continuing with the reset
            {
                auto lock = details::s_detourLock.lock_exclusive();
                m_removed = true;
                while (m_entryCount > 0)
                    m_invokeComplete.wait(lock);

                auto entryPtr = &s_globalInstance;
                while (*entryPtr && (*entryPtr != this))
                    entryPtr = &(*entryPtr)->m_next;

                // Failing this check likely means that there's either a memory corruption issue or an error in our logic
                FAIL_FAST_IF_NULL(*entryPtr);
                *entryPtr = m_next;

                if (--s_refCount == 0)
                {
                    RETURN_IF_FAILED(details::Unregister(&s_target, &base::callback));
                }
            }

            m_detour = {};
        }

        return S_OK;
    }

    template <typename Func>
    HRESULT reset(Func&& func) noexcept(
        (wistd::is_reference_v<Func> || wistd::is_const_v<Func>) ? wistd::is_nothrow_copy_assignable_v<wistd::remove_reference_t<Func>>
                                                                 : wistd::is_nothrow_move_assignable_v<Func>)
    {
        RETURN_IF_FAILED(reset());

        // Once we release the lock below, we need to ensure that 'm_detour' is set because another thread may try and call into
        // it. We can't do the assignment under the lock however, because the copy/move of 'func' may end up calling the function
        // that we're detouring, leading to deadlock.
        m_detour = wistd::forward<Func>(func);
        auto resetOnExit = wil::scope_exit([&] {
            m_detour = {};
        });

        {
            auto lock = details::s_detourLock.lock_exclusive();
            if (s_refCount == 0)
            {
                RETURN_IF_FAILED(details::Register(&s_target, &base::callback));
            }
            ++s_refCount;

            // 's_globalInstance' must be read/set under the lock
            m_next = s_globalInstance;
            s_globalInstance = this;
        }

        resetOnExit.release();
        return S_OK;
    }

private:
    template <typename... ArgsT>
    static return_type invoke_callback(ArgsT&&... args) noexcept(base::is_noexcept)
    {
        detoured_global_function* target = nullptr;
        {
            auto lock = details::s_detourLock.lock_exclusive();
            if (s_invoking)
            {
                target = s_invoking->m_next;
            }
            else
            {
                target = s_globalInstance;
            }

            while (target && target->m_removed)
            {
                target = target->m_next;
            }

            if (target)
            {
                ++target->m_entryCount;
            }
        }

        if (target)
        {
            auto oldInvoking = s_invoking;
            auto cleanup = wil::scope_exit([&] {
                s_invoking = oldInvoking;

                auto lock = details::s_detourLock.lock_exclusive();
                if (--target->m_entryCount == 0)
                {
                    target->m_invokeComplete.notify_all();
                }
            });
            s_invoking = target;
            return target->m_detour(wistd::forward<ArgsT>(args)...);
        }
        // NOTE: We specifically don't want to set 's_invoking' to null when forwarding to the implementation since we also
        // use that to signal that the thread is not invoking any function.

        // All registered functions have been called; forward to the implementation
        return s_target(wistd::forward<ArgsT>(args)...);
    }

    detoured_global_function* m_next = nullptr; // Linked list of registrations; guarded by s_detourLock
    wistd::function<function_type> m_detour;
    int m_entryCount = 0;                     // Keep track of how many threads are invoking the detour; used to delay destruction
    bool m_removed = false;                   // Marks the node as removed; not actually removed until all invocations complete
    wil::condition_variable m_invokeComplete; // Used to delay destruction until all threads have finished invoking the detour

    static inline detoured_global_function* s_globalInstance = nullptr;        // Linked list head; guarded by s_detourLock
    static inline thread_local detoured_global_function* s_invoking = nullptr; // Used for reentrancy
    static inline auto s_target = TargetFn;
    static inline int s_refCount = 0; // Guarded by details::s_detourLock
};

//! An RAII type that manages a thread-specific detouring of a specific function.
//! This type is used to register a specific lambda (or function pointer) as the detour for a specific function that only gets
//! invoked on the thread it was created on. This helps avoid issues such as the lambda being invoked on an unexpected thread or
//! having it get destroyed while being invoked on a different thread.
template <auto TargetFn>
struct detoured_thread_function : details::detoured_thread_function_base<detoured_thread_function<TargetFn>, decltype(TargetFn)>
{
private:
    using base = details::detoured_thread_function_base<detoured_thread_function, decltype(TargetFn)>;
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
        wistd::swap_wil(m_detour, other.m_detour);
        wistd::swap_wil(m_reentry, other.m_reentry);

        // We need to swap positions in the linked list
        detoured_thread_function** thisPos = nullptr;
        detoured_thread_function** otherPos = nullptr;
        for (auto ptr = &s_threadInstance; *ptr; ptr = &(*ptr)->m_next)
        {
            if (*ptr == this)
                thisPos = ptr;
            else if (*ptr == &other)
                otherPos = ptr;
        }

        if (!thisPos)
        {
            // This not in the list; put it in other's position
            if (otherPos)
            {
                *otherPos = this;
                wistd::swap_wil(m_next, other.m_next);
            }
        }
        else if (!otherPos)
        {
            // Other not in the list; put it in this's position
            *thisPos = &other;
            wistd::swap_wil(m_next, other.m_next);
        }
        // Otherwise, both are in the list
        else if (m_next == &other)
        {
            // Special case; other needs to point to this now
            WI_ASSERT(otherPos == &m_next);
            *thisPos = &other;
            m_next = wistd::exchange(other.m_next, this);
        }
        else if (other.m_next == this)
        {
            // Same as above, just swapped
            WI_ASSERT(thisPos == &other.m_next);
            *otherPos = this;
            other.m_next = wistd::exchange(m_next, &other);
        }
        else
        {
            // General "easy" case; we can swap pointers
            wistd::swap_wil(*thisPos, *otherPos);
            wistd::swap_wil(m_next, other.m_next);
        }
    }

    HRESULT reset() noexcept
    {
        if (m_detour)
        {
            m_detour = {};

            detoured_thread_function** entryPtr = &s_threadInstance;
            while (*entryPtr && (*entryPtr != this))
                entryPtr = &(*entryPtr)->m_next;

            // Faling this check would likely imply that this object is being destroyed on the wrong thread. No matter the reason,
            // this should be considered a pretty fatal error
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
    template <typename... ArgsT>
    static return_type invoke_callback(ArgsT&&... args) noexcept(base::is_noexcept)
    {
        for (auto ptr = s_threadInstance; ptr; ptr = ptr->m_next)
        {
            if (!ptr->m_reentry)
            {
                return ptr->invoke(wistd::forward<ArgsT>(args)...);
            }
        }

        // All registered functions have been called; forward to the implementation
        return s_target(wistd::forward<ArgsT>(args)...);
    }

    template <typename... ArgsT>
    return_type invoke(ArgsT&&... args) noexcept(base::is_noexcept)
    {
        WI_ASSERT(!m_reentry);
        m_reentry = true;
        auto resetOnExit = wil::scope_exit([&] { // No guarantee that 'return_type' is a movable type; NRVO is not guaranteed
            m_reentry = false;
        });
        return m_detour(wistd::forward<ArgsT>(args)...);
    }

    detoured_thread_function* m_next = nullptr; // Linked list of registrations; support detouring more than once on same thread
    wistd::function<function_type> m_detour;
    bool m_reentry = false; // Used to handle forwarding to impl or the detour

    static inline thread_local detoured_thread_function* s_threadInstance = nullptr; // Linked list head
    static inline auto s_target = TargetFn;
    static inline int s_refCount = 0; // Guarded by details::s_detourLock
};
} // namespace witest
