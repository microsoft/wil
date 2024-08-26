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
//! STL-like allocators for various memory allocation schemes
#ifndef __WIL_ALLOCATORS_INCLUDED
#define __WIL_ALLOCATORS_INCLUDED

#include "result_macros.h"

#include <objbase.h>

namespace wil
{
/// @cond
namespace details
{
    // NOTE: For some reason, clang-cl chokes if the following
    template <typename Alloc, typename T>
    struct rebind_alloc_t;

    template <template <typename...> typename Alloc, typename T, typename U, typename... Rest>
    struct rebind_alloc_t<Alloc<U, Rest...>, T>
    {
        using other = Alloc<T, Rest...>;
    };

    template <typename Alloc, typename T>
    static typename Alloc::template rebind<T>::other deduce_rebind(int);
    template <typename Alloc, typename T>
    static typename rebind_alloc_t<Alloc, T>::other deduce_rebind(...);

    // Helper type for implementing containers that have an allocator member that might be empty, in which case derivation allows
    // object size optimization since we don't need to waste a byte on a unique address.
    // NOTE: [[no_unique_address]] would be better, but (1) this requires C++20, and (2) sometimes has issues with MSVC
    template <typename Alloc, typename = void>
    struct allocator_aware_container_base
    {
    private: // NOTE: Private since consuming code should not depend on whether or not this is a member
        Alloc m_alloc;

    public:

        constexpr allocator_aware_container_base() noexcept = default;

        constexpr allocator_aware_container_base(const Alloc& alloc) noexcept(wistd::is_nothrow_copy_constructible_v<Alloc>) : m_alloc(alloc)
        {
        }

        constexpr allocator_aware_container_base(Alloc&& alloc) noexcept(wistd::is_nothrow_move_constructible_v<Alloc>) :
            m_alloc(wistd::move(alloc))
        {
        }

        constexpr Alloc get_allocator() const noexcept(wistd::is_nothrow_copy_constructible_v<Alloc>)
        {
            return m_alloc;
        }

    protected:

        constexpr Alloc& alloc() noexcept
        {
            return m_alloc;
        }

        constexpr const Alloc& alloc() const noexcept
        {
            return m_alloc;
        }
    };

    template <typename Alloc>
    struct allocator_aware_container_base<Alloc, wistd::enable_if_t<!wistd::is_final_v<Alloc>>> : private Alloc
    {
    public:

        constexpr allocator_aware_container_base() noexcept = default;

        constexpr allocator_aware_container_base(const Alloc& alloc) noexcept(wistd::is_nothrow_constructible_v<Alloc>) :
            Alloc(alloc)
        {
        }

        constexpr allocator_aware_container_base(Alloc&& alloc) noexcept(wistd::is_nothrow_move_constructible_v<Alloc>) :
            Alloc(wistd::move(alloc))
        {
        }

        constexpr Alloc get_allocator() const noexcept(wistd::is_nothrow_copy_constructible_v<Alloc>)
        {
            return *this;
        }

    protected:

        constexpr Alloc& alloc() noexcept
        {
            return *this;
        }

        constexpr const Alloc& alloc() const noexcept
        {
            return *this;
        }
    };
}
/// @endcond

// A substitute for std::allocator_traits to avoid dependencies on STL types. This also understands the notion of possibly having
// non-exceptional allocator types, which the STL does not
template <typename Alloc>
struct allocator_traits
{
    using allocator_type = Alloc;
    using value_type = typename Alloc::value_type;

private:
    template <typename AllocT = Alloc>
    static typename AllocT::pointer deduce_pointer(int);
    template <typename AllocT = Alloc>
    static value_type* deduce_pointer(...);
public:
    using pointer = decltype(deduce_pointer(0));

private:
    // NOTE: This should go through std::pointer_traits... consider if this has any gotchas
    template <typename AllocT = Alloc>
    static typename AllocT::const_pointer deduce_const_pointer(int);
    template <typename AllocT = Alloc>
    static wistd::add_const_t<value_type>* deduce_const_pointer(...);
public:
    using const_pointer = decltype(deduce_const_pointer(0));

private:
    // NOTE: This should go through std::pointer_traits... consider if this has any gotchas
    template <typename AllocT = Alloc>
    static typename AllocT::void_pointer deduce_void_pointer(int);
    template <typename AllocT = Alloc>
    static void* deduce_void_pointer(...);
public:
    using void_pointer = decltype(deduce_void_pointer(0));

private:
    // NOTE: This should go through std::pointer_traits... consider if this has any gotchas
    template <typename AllocT = Alloc>
    static typename AllocT::const_void_pointer deduce_const_void_pointer(int);
    template <typename AllocT = Alloc>
    static const void* deduce_const_void_pointer(...);
public:
    using const_void_pointer = decltype(deduce_const_void_pointer(0));

private:
    // NOTE: This should go through std::pointer_traits... consider if this has any gotchas
    template <typename AllocT = Alloc>
    static typename AllocT::difference_type deduce_difference_type(int);
    template <typename AllocT = Alloc>
    static ptrdiff_t deduce_difference_type(...);
public:
    using difference_type = decltype(deduce_difference_type(0));

private:
    template <typename AllocT = Alloc>
    static typename AllocT::size_type deduce_size_type(int);
    template <typename AllocT = Alloc>
    static wistd::make_unsigned_t<difference_type> deduce_size_type(...);
public:
    using size_type = decltype(deduce_size_type(0));

private:
    template <typename AllocT = Alloc>
    static typename AllocT::propagate_on_container_copy_assignment deduce_propagate_on_container_copy_assignment(int);
    template <typename AllocT = Alloc>
    static wistd::false_type deduce_propagate_on_container_copy_assignment(...);
public:
    using propagate_on_container_copy_assignment = decltype(deduce_propagate_on_container_copy_assignment(0));

private:
    template <typename AllocT = Alloc>
    static typename AllocT::propagate_on_container_move_assignment deduce_propagate_on_container_move_assignment(int);
    template <typename AllocT = Alloc>
    static wistd::false_type deduce_propagate_on_container_move_assignment(...);
public:
    using propagate_on_container_move_assignment = decltype(deduce_propagate_on_container_move_assignment(0));

private:
    template <typename AllocT = Alloc>
    static typename AllocT::propagate_on_container_swap deduce_propagate_on_container_swap(int);
    template <typename AllocT = Alloc>
    static wistd::false_type deduce_propagate_on_container_swap(...);
public:
    using propagate_on_container_swap = decltype(deduce_propagate_on_container_swap(0));

private:
    template <typename AllocT = Alloc>
    static typename AllocT::is_always_equal deduce_is_always_equal(int);
    template <typename AllocT = Alloc>
    static wistd::is_empty<AllocT> deduce_is_always_equal(...);
public:
    using is_always_equal = decltype(deduce_is_always_equal(0));

    template <typename T>
    using rebind_alloc = decltype(details::deduce_rebind<Alloc, T>(0));

    template <typename T>
    using rebind_traits = allocator_traits<rebind_alloc<T>>;

    static constexpr pointer allocate(Alloc& alloc, size_type count) noexcept(noexcept(alloc.allocate(count)))
    {
        return alloc.allocate(count);
    }

private:
    template <typename AllocT = Alloc>
    static auto has_allocate_hint(int)
        -> details::first_t<wistd::true_type, decltype(wistd::declval<AllocT>().allocate(0, wistd::declval<const_void_pointer>()))>;
    template <typename AllocT = Alloc>
    static wistd::false_type has_allocate_hint(...);
public:
    // NOTE: Ideally we should determine which 'allocate' function we'll call and set 'noexcept' based off that, however this
    // should be fine for most, if not all, scenarios
    static constexpr pointer allocate(Alloc& alloc, size_type count, const_void_pointer hint) noexcept(noexcept(alloc.allocate(count)))
    {
        if constexpr (decltype(has_allocate_hint(0))::value)
        {
            return alloc.allocate(count, hint);
        }
        else
        {
            return alloc.allocate(count);
        }
    }

    static constexpr void deallocate(Alloc& alloc, pointer ptr, size_type count) noexcept
    {
        return alloc.deallocate(ptr, count);
    }

private:
    template <typename AllocT, typename T, typename... Args>
    static auto has_construct(int)
        -> details::first_t<wistd::true_type, decltype(wistd::declval<AllocT>().construct(wistd::declval<T*>(), wistd::declval<Args>()...))>;
    template <typename AllocT, typename T, typename... Args>
    static wistd::false_type has_construct(...);
public:
    // NOTE: Ideally we should determine if we'll call 'construct' and use that to set 'noexcept when present, however this should
    // be fine for most, if not all, scenarios
    template <typename T, typename... Args>
    static constexpr void construct(Alloc& alloc, T* ptr, Args&&... args) noexcept(wistd::is_nothrow_constructible_v<T, Args...>)
    {
        if constexpr (decltype(has_construct<Alloc, T, Args...>(0))::value)
        {
            alloc.construct(ptr, wistd::forward<Args>(args)...);
        }
        else
        {
            (void)alloc;
            ::new (static_cast<void*>(ptr)) T(wistd::forward<Args>(args)...);
        }
    }

private:
    template <typename AllocT, typename T>
    static auto has_destroy(int) -> details::first_t<wistd::true_type, decltype(wistd::declval<AllocT>().destroy(wistd::declval<T*>()))>;
    template <typename AllocT, typename T>
    static wistd::false_type has_destroy(...);
public:
    // NOTE: Ideally we should determine if we'll call 'destroy' and use that to set 'noexcept when present, however this should
    // be fine for most, if not all, scenarios
    template <typename T>
    static constexpr void destroy(Alloc& alloc, T* ptr) noexcept(wistd::is_nothrow_destructible_v<T>)
    {
        if constexpr (decltype(has_destroy<Alloc, T>(0))::value)
        {
            alloc.destroy(ptr);
        }
        else
        {
            (void)alloc;
            ptr->~T();
        }
    }

private:
    template <typename AllocT = Alloc>
    static auto has_max_size(int) -> details::first_t<wistd::true_type, decltype(wistd::declval<const AllocT>().max_size())>;
    template <typename AllocT = Alloc>
    static wistd::false_type has_max_size(...);
public:
    static constexpr size_type max_size(const Alloc& alloc) noexcept
    {
        if constexpr (decltype(has_max_size(0))::value)
        {
            return alloc.max_size();
        }
        else
        {
            // NOTE: size_type is unsigned, so overflow is well-defined behavior
            (void)alloc;
            return (static_cast<size_type>(0) - 1) / sizeof(value_type);
        }
    }

private:
    template <typename AllocT = Alloc>
    static auto has_select_on_container_copy_construction(int)
        -> details::first_t<wistd::true_type, decltype(wistd::declval<const AllocT>().select_on_container_copy_construction())>;
    template <typename AllocT = Alloc>
    static wistd::false_type has_select_on_container_copy_construction(...);
public:
    static constexpr Alloc select_on_container_copy_construction(const Alloc& alloc) // TODO: noexcept?
    {
        if constexpr (decltype(has_select_on_container_copy_construction(0))::value)
        {
            return alloc.select_on_container_copy_construction();
        }
        else
        {
            return alloc;
        }
    }

    // NOTE: The following functions are not implemented because they rely on STL types:
    //      * allocate_at_least                 -   Requires std::allocation_result

    // Custom trait exposing the error policy of the allocator, or 'err_exception_policy' if not present
private:
    template <typename AllocT = Alloc>
    static typename AllocT::err_policy deduce_err_policy(int);
    template <typename AllocT = Alloc>
    static err_exception_policy deduce_err_policy(...);
public:
    using err_policy = decltype(deduce_err_policy(0));
};

/// @cond
namespace details
{
    // Used to easily provide nothrow/failfast/exceptional allocators through a single 'Alloc'. This type is epxected to have the
    // following interface:
    //      T* do_allocate(size_t count)        Returns null on failure
    template <template <typename, typename> typename Alloc, typename T, typename ErrPolicy>
    struct allocator_impl_t
    {
    private:
        using Allocator = Alloc<T, ErrPolicy>;
        using ErrTraits = wil::err_policy_traits<ErrPolicy>;

    public:
        using err_policy = ErrPolicy;
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        constexpr T* allocate(size_type count) noexcept(ErrTraits::is_nothrow)
        {
            T* result = nullptr;

            constexpr auto max_size = (static_cast<size_type>(0) - 1) / sizeof(T);
            if (count <= max_size)
            {
                result = static_cast<Allocator*>(this)->do_allocate(count);
            }

            ErrTraits::Pointer(result);
            return result;
        }
    };

    // Stateless allocators have a few more optimizations available to them
    template <template <typename, typename> typename Alloc, typename T, typename ErrPolicy>
    struct stateless_allocator_impl_t : allocator_impl_t<Alloc, T, ErrPolicy>
    {
        // Used by container types to make move assignment 'noexcept' since otherwise they would need a runtime test for equality
        // to check and see if they need to allocate new storage
        using propagate_on_container_move_assignment = wistd::true_type;

        constexpr stateless_allocator_impl_t() noexcept = default;

        template <typename U, typename ErrPolicyOther>
        constexpr stateless_allocator_impl_t(const Alloc<U, ErrPolicyOther>&) noexcept
        {
        }

        friend constexpr bool operator==(const Alloc<T, ErrPolicy>&, const Alloc<T, ErrPolicy>&) noexcept
        {
            return true;
        }

        friend constexpr bool operator!=(const Alloc<T, ErrPolicy>&, const Alloc<T, ErrPolicy>&) noexcept
        {
            return false;
        }
    };
}
/// @endcond

#ifdef WIL_ENABLE_EXCEPTIONS
// Basically std::allocaotr, but we can refer to it directly even when STL use cannot be assumed
template <typename T, typename ErrPolicy = err_exception_policy>
struct new_delete_allocator_t : details::stateless_allocator_impl_t<new_delete_allocator_t, T, ErrPolicy>
{
private:
    using Base = details::stateless_allocator_impl_t<new_delete_allocator_t, T, ErrPolicy>;
    friend details::allocator_impl_t<new_delete_allocator_t, T, ErrPolicy>;

    T* do_allocate(typename Base::size_type count) noexcept
    {
        // NOTE: 'allocator_impl_t' is in charge of ensuring this doesn't overflow
        return static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t{alignof(T)}, std::nothrow));
    }

public:

    using Base::Base;

    void deallocate(T* ptr, typename Base::size_type count) noexcept
    {
        ::operator delete(ptr, count, std::align_val_t{alignof(T)});
    }
};

template <typename T>
using new_delete_allocator = new_delete_allocator_t<T, err_exception_policy>;
template <typename T>
using new_delete_allocator_nothrow = new_delete_allocator_t<T, err_returncode_policy>;
template <typename T>
using new_delete_allocator_failfast = new_delete_allocator_t<T, err_failfast_policy>;
#endif

template <typename T, typename ErrPolicy = err_exception_policy>
struct cotaskmem_allocator_t : details::stateless_allocator_impl_t<cotaskmem_allocator_t, T, ErrPolicy>
{
private:
    static_assert(alignof(T) <= MEMORY_ALLOCATION_ALIGNMENT , "Type cannot be properly aligned by CoTaskMemAlloc");

    using Base = details::stateless_allocator_impl_t<cotaskmem_allocator_t, T, ErrPolicy>;
    friend details::allocator_impl_t<cotaskmem_allocator_t, T, ErrPolicy>;

    T* do_allocate(typename Base::size_type count) noexcept
    {
        return static_cast<T*>(::CoTaskMemAlloc(count * sizeof(T)));
    }

public:

    using Base::Base;

    void deallocate(T* ptr, typename Base::size_type) noexcept
    {
        ::CoTaskMemFree(ptr);
    }
};

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename T>
using cotaskmem_allocator = cotaskmem_allocator_t<T, err_exception_policy>;
#endif
template <typename T>
using cotaskmem_allocator_nothrow = cotaskmem_allocator_t<T, err_returncode_policy>;
template <typename T>
using cotaskmem_allocator_failfast = cotaskmem_allocator_t<T, err_failfast_policy>;

template <typename T, typename ErrPolicy = err_exception_policy>
struct process_heap_allocator_t : details::stateless_allocator_impl_t<process_heap_allocator_t, T, ErrPolicy>
{
private:
    static_assert(alignof(T) <= MEMORY_ALLOCATION_ALIGNMENT , "Type cannot be properly aligned by HeapAlloc");

    using Base = details::stateless_allocator_impl_t<process_heap_allocator_t, T, ErrPolicy>;
    friend details::allocator_impl_t<process_heap_allocator_t, T, ErrPolicy>;

    T* do_allocate(typename Base::size_type count) noexcept
    {
        return static_cast<T*>(::HeapAlloc(::GetProcessHeap(), 0, count * sizeof(T)));
    }

public:

    using Base::Base;

    void deallocate(T* ptr, typename Base::size_type) noexcept
    {
        ::HeapFree(::GetProcessHeap(), 0, ptr);
    }
};

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename T>
using process_heap_allocator = process_heap_allocator_t<T, err_exception_policy>;
#endif
template <typename T>
using process_heap_allocator_nothrow = process_heap_allocator_t<T, err_returncode_policy>;
template <typename T>
using process_heap_allocator_failfast = process_heap_allocator_t<T, err_failfast_policy>;

template <typename T, typename ErrPolicy = err_exception_policy>
struct heap_allocator_t : details::allocator_impl_t<heap_allocator_t, T, ErrPolicy>
{
private:
    static_assert(alignof(T) <= MEMORY_ALLOCATION_ALIGNMENT , "Type cannot be properly aligned by HeapAlloc");

    using Base = details::allocator_impl_t<heap_allocator_t, T, ErrPolicy>;
    friend Base;

    template <typename, typename>
    friend struct heap_allocator_t;

    T* do_allocate(typename Base::size_type count) noexcept
    {
        return static_cast<T*>(::HeapAlloc(m_heap, 0, count * sizeof(T)));
    }

    // ***NON OWNING***
    HANDLE m_heap;

public:
    // NOTE: We don't specify any 'propagate_on_container_*' aliases. It is assumed that the heap that a container is created with
    // should persist for the lifetime of that container

    constexpr heap_allocator_t(HANDLE heap) : m_heap(heap)
    {
    }

    template <typename U, typename ErrPolicyOther>
    constexpr heap_allocator_t(const heap_allocator_t<U, ErrPolicyOther>& other) noexcept : m_heap(other.m_heap)
    {
    }

    void deallocate(T* ptr, typename Base::size_type) noexcept
    {
        ::HeapFree(m_heap, 0, ptr);
    }

    friend constexpr bool operator==(const heap_allocator_t& lhs, const heap_allocator_t& rhs) noexcept
    {
        return lhs.m_heap == rhs.m_heap;
    }

    friend constexpr bool operator!=(const heap_allocator_t& lhs, const heap_allocator_t& rhs) noexcept
    {
        return lhs.m_heap != rhs.m_heap;
    }
};

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename T>
using heap_allocator = heap_allocator_t<T, err_exception_policy>;
#endif
template <typename T>
using heap_allocator_nothrow = heap_allocator_t<T, err_returncode_policy>;
template <typename T>
using heap_allocator_failfast = heap_allocator_t<T, err_failfast_policy>;

template <typename T, typename ErrPolicy = err_exception_policy>
struct virtual_allocator_t : details::stateless_allocator_impl_t<virtual_allocator_t, T, ErrPolicy>
{
private:
    static_assert(alignof(T) <= MEMORY_ALLOCATION_ALIGNMENT , "Type cannot be properly aligned by VirtualAlloc");

    using Base = details::stateless_allocator_impl_t<virtual_allocator_t, T, ErrPolicy>;
    friend details::allocator_impl_t<virtual_allocator_t, T, ErrPolicy>;

    T* do_allocate(typename Base::size_type count) noexcept
    {
        return static_cast<T*>(::VirtualAlloc(nullptr, count * sizeof(T), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    }

public:

    using Base::Base;

    void deallocate(T* ptr, typename Base::size_type) noexcept
    {
        ::VirtualFree(ptr, 0, MEM_RELEASE);
    }
};

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename T>
using virtual_allocator = virtual_allocator_t<T, err_exception_policy>;
#endif
template <typename T>
using virtual_allocator_nothrow = virtual_allocator_t<T, err_returncode_policy>;
template <typename T>
using virtual_allocator_failfast = virtual_allocator_t<T, err_failfast_policy>;

template <typename T, typename ErrPolicy = err_exception_policy>
struct local_allocator_t : details::stateless_allocator_impl_t<local_allocator_t, T, ErrPolicy>
{
private:
    static_assert(alignof(T) <= MEMORY_ALLOCATION_ALIGNMENT , "Type cannot be properly aligned by LocalAlloc");

    using Base = details::stateless_allocator_impl_t<local_allocator_t, T, ErrPolicy>;
    friend details::allocator_impl_t<local_allocator_t, T, ErrPolicy>;

    T* do_allocate(typename Base::size_type count) noexcept
    {
        return static_cast<T*>(::LocalAlloc(LMEM_FIXED, count * sizeof(T)));
    }

public:

    using Base::Base;

    void deallocate(T* ptr, typename Base::size_type) noexcept
    {
        ::LocalFree(ptr);
    }
};

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename T>
using local_allocator = local_allocator_t<T, err_exception_policy>;
#endif
template <typename T>
using local_allocator_nothrow = local_allocator_t<T, err_returncode_policy>;
template <typename T>
using local_allocator_failfast = local_allocator_t<T, err_failfast_policy>;

template <typename T, typename ErrPolicy = err_exception_policy>
struct global_allocator_t : details::stateless_allocator_impl_t<global_allocator_t, T, ErrPolicy>
{
private:
    static_assert(alignof(T) <= MEMORY_ALLOCATION_ALIGNMENT , "Type cannot be properly aligned by GlobalAlloc");

    using Base = details::stateless_allocator_impl_t<global_allocator_t, T, ErrPolicy>;
    friend details::allocator_impl_t<global_allocator_t, T, ErrPolicy>;

    T* do_allocate(typename Base::size_type count) noexcept
    {
        return static_cast<T*>(::GlobalAlloc(GMEM_FIXED, count * sizeof(T)));
    }

public:

    using Base::Base;

    void deallocate(T* ptr, typename Base::size_type) noexcept
    {
        ::GlobalFree(ptr);
    }
};

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename T>
using global_allocator = global_allocator_t<T, err_exception_policy>;
#endif
template <typename T>
using global_allocator_nothrow = global_allocator_t<T, err_returncode_policy>;
template <typename T>
using global_allocator_failfast = global_allocator_t<T, err_failfast_policy>;
}

#endif // __WIL_ALLOCATORS_INCLUDED
