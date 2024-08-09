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

namespace wil
{
// A substitute for std::allocator_traits to avoid dependencies on STL types. This also understands the notion of possibly having
// non-exceptional allocator types, which the STL does not
template <typename AllocT>
struct allocator_traits
{
    using allocator_type = AllocT;
    using value_type = typename AllocT::value_type;

    template <typename Alloc = AllocT>
    static typename Alloc::pointer deduce_pointer(int);
    template <typename Alloc = AllocT>
    static value_type* deduce_pointer(...);
    using pointer = decltype(deduce_pointer(0));

    // NOTE: This should go through std::pointer_traits... consider if this has any gotchas
    template <typename Alloc = AllocT>
    static typename Alloc::const_pointer deduce_const_pointer(int);
    template <typename Alloc = AllocT>
    static wistd::add_const_t<value_type>* deduce_const_pointer(...);
    using const_pointer = decltype(deduce_const_pointer(0));

    // NOTE: This should go through std::pointer_traits... consider if this has any gotchas
    template <typename Alloc = AllocT>
    static typename Alloc::void_pointer deduce_void_pointer(int);
    template <typename Alloc = AllocT>
    static void* deduce_void_pointer(...);
    using void_pointer = decltype(deduce_void_pointer(0));

    // NOTE: This should go through std::pointer_traits... consider if this has any gotchas
    template <typename Alloc = AllocT>
    static typename Alloc::const_void_pointer deduce_const_void_pointer(int);
    template <typename Alloc = AllocT>
    static const void* deduce_const_void_pointer(...);
    using const_void_pointer = decltype(deduce_const_void_pointer(0));

    // NOTE: This should go through std::pointer_traits... consider if this has any gotchas
    template <typename Alloc = AllocT>
    static typename Alloc::difference_type deduce_difference_type(int);
    template <typename Alloc = AllocT>
    static ptrdiff_t deduce_difference_type(...);
    using difference_type = decltype(deduce_difference_type(0));

    template <typename Alloc = AllocT>
    static typename Alloc::size_type deduce_size_type(int);
    template <typename Alloc = AllocT>
    static wistd::make_unsigned_t<difference_type> deduce_size_type(...);
    using size_type = decltype(deduce_size_type(0));

    template <typename Alloc = AllocT>
    static typename Alloc::propagate_on_container_copy_assignment deduce_propagate_on_container_copy_assignment(int);
    template <typename Alloc = AllocT>
    static wistd::false_type deduce_propagate_on_container_copy_assignment(...);
    using propagate_on_container_copy_assignment = decltype(deduce_propagate_on_container_copy_assignment(0));

    template <typename Alloc = AllocT>
    static typename Alloc::propagate_on_container_move_assignment deduce_propagate_on_container_move_assignment(int);
    template <typename Alloc = AllocT>
    static wistd::false_type deduce_propagate_on_container_move_assignment(...);
    using propagate_on_container_move_assignment = decltype(deduce_propagate_on_container_move_assignment(0));

    template <typename Alloc = AllocT>
    static typename Alloc::propagate_on_container_swap deduce_propagate_on_container_swap(int);
    template <typename Alloc = AllocT>
    static wistd::false_type deduce_propagate_on_container_swap(...);
    using propagate_on_container_swap = decltype(deduce_propagate_on_container_swap(0));

    template <typename Alloc = AllocT>
    static typename Alloc::is_always_equal deduce_is_always_equal(int);
    template <typename Alloc = AllocT>
    static wistd::is_empty<Alloc> deduce_is_always_equal(...);
    using is_always_equal = decltype(deduce_is_always_equal(0));

    // TODO: rebind_alloc<T>
    // TODO: rebind_traits<T>

    static constexpr pointer allocate(AllocT& alloc, size_type count) noexcept(noexcept(alloc.allocate(count)))
    {
        return alloc.allocate(count);
    }

    template <typename Alloc = AllocT>
    static auto has_allocate_hint(int)
        -> details::first_t<wistd::true_type, decltype(wistd::declval<Alloc>().allocate(0, wistd::declval<const_void_pointer>()))>;
    template <typename Alloc = AllocT>
    static wistd::false_type has_allocate_hint(...);

    // NOTE: Ideally we should determine which 'allocate' function we'll call and set 'noexcept' based off that, however this
    // should be fine for most, if not all, scenarios
    static constexpr pointer allocate(AllocT& alloc, size_type count, const_void_pointer hint) noexcept(noexcept(alloc.allocate(count)))
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

    static constexpr void deallocate(AllocT& alloc, pointer ptr, size_type count) noexcept
    {
        return alloc.deallocate(ptr, count);
    }

    template <typename Alloc, typename T, typename... Args>
    static auto has_construct(int)
        -> details::first_t<wistd::true_type, decltype(wistd::declval<Alloc>().construct(wistd::declval<T*>(), wistd::declval<Args>()...))>;
    template <typename Alloc, typename T, typename... Args>
    static wistd::false_type has_construct(...);

    // NOTE: Ideally we should determine if we'll call 'construct' and use that to set 'noexcept when present, however this should
    // be fine for most, if not all, scenarios
    template <typename T, typename... Args>
    static constexpr void construct(AllocT& alloc, T* ptr, Args&&... args) noexcept(wistd::is_nothrow_constructible_v<T, Args...>)
    {
        if constexpr (decltype(has_construct<AllocT, T, Args...>(0))::value)
        {
            alloc.construct(ptr, wistd::forward<Args>(args)...);
        }
        else
        {
            (void)alloc;
            ::new (static_cast<void*>(ptr)) T(wistd::forward<Args>(args)...);
        }
    }

    template <typename Alloc, typename T>
    static auto has_destroy(int) -> details::first_t<wistd::true_type, decltype(wistd::declval<Alloc>().destroy(wistd::declval<T*>()))>;
    template <typename Alloc, typename T>
    static wistd::false_type has_destroy(...);

    // NOTE: Ideally we should determine if we'll call 'destroy' and use that to set 'noexcept when present, however this should
    // be fine for most, if not all, scenarios
    template <typename T>
    static constexpr void destroy(AllocT& alloc, T* ptr) noexcept(wistd::is_nothrow_destructible_v<T>)
    {
        if constexpr (decltype(has_destroy<AllocT, T>(0))::value)
        {
            alloc.destroy(ptr);
        }
        else
        {
            (void)alloc;
            ptr->~T();
        }
    }

    template <typename Alloc = AllocT>
    static auto has_max_size(int) -> details::first_t<wistd::true_type, decltype(wistd::declval<Alloc>().max_size())>;
    template <typename Alloc = AllocT>
    static wistd::false_type has_max_size(...);

    static constexpr size_type max_size(const AllocT& alloc) noexcept
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

    template <typename Alloc = AllocT>
    static auto has_select_on_container_copy_construction(int)
        -> details::first_t<wistd::true_type, decltype(wistd::declval<Alloc>().select_on_container_copy_construction(wistd::declval<Alloc>()))>;
    static wistd::false_type has_select_on_container_copy_construction(...);

    static constexpr AllocT select_on_container_copy_construction(const AllocT& alloc) // TODO: noexcept?
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
};
#if 0
/// @cond
namespace details
{
    // Most allocators should have 'propagate_on_container_move_assignment' set to true, so we deduce as such when not present on
    // the base type. This helps the deduction
    template <typename AllocatorT>
    struct deduce_propagate_on_container_move_assignment_t
    {
        template <typename T = AllocatorT>
        static typename T::propagate_on_container_move_assignment evaluate(int);

        template <typename T = AllocatorT>
        static wistd::true_type evaluate(...);

        using type = decltype(evaluate(0));
    };

    template <typename AllocatorT>
    using deduce_propagate_on_container_move_assignment = typename deduce_propagate_on_container_move_assignment_t<AllocatorT>::type;

    // Used to easily provide nothrow/failfast/exceptional allocators through a single base
    template <typename AllocatorT, typename T, typename err_policy>
    struct allocator_impl_t : protected AllocatorT
    {
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = deduce_propagate_on_container_move_assignment<AllocatorT>;

        constexpr allocator_impl_t() noexcept = default;
        constexpr allocator_impl_t(const allocator_impl_t<AllocatorT, T, err_policy>&) noexcept
        {
            // TODO: Maybe not valid for all allocator types?
        }

        constexpr T* allocate(size_t count) noexcept(details::is_error_policy_nothrow<err_policy>)
        {
            auto result = do_allocate();
            if (!result)
            {
                // NOTE: May just return the same HRESULT; that's fine as we're expecting to be able to return null
                err_policy::HResult(E_OUTOFMEMORY);
            }

            return result;
        }

        constexpr void deallocate(T* ptr, size_t count) noexcept
        {
            do_deallocate(ptr, count);
        }

        // NOTE: The following functions are not implemented because the standarad has deprecated/removed them:
        //      * address                           -   Deprecated in C++17 / Removed in C++20
        //      * allocate(size_t, const void*)     -   Deprecated in C++17 / Removed in C++20
        //      * max_size                          -   Deprecated in C++17 / Removed in C++20
        //      * construct                         -   Deprecated in C++17 / Removed in C++20
        //      * destroy                           -   Deprecated in C++17 / Removed in C++20

        // NOTE: The following functions are not implemented because they rely on STL types:
        //      * allocate_at_least                 -   Requires std::allocation_result
    };

    template <typename AllocatorT, typename LhsT, typename LhsErrPolicy, typename RhsT, typename RhsErrPolicy>
    inline constexpr bool operator==(
        const allocator_impl_t<AllocatorT, LhsT, LhsErrPolicy>&, const allocator_impl_t<AllocatorT, RhsT, RhsErrPolicy>&) noexcept
    {
        return true;
    }

    template <typename AllocatorT, typename LhsT, typename LhsErrPolicy, typename RhsT, typename RhsErrPolicy>
    inline constexpr bool operator!=(
        const allocator_impl_t<AllocatorT, LhsT, LhsErrPolicy>&, const allocator_impl_t<AllocatorT, RhsT, RhsErrPolicy>&) noexcept
    {
        return false;
    }
}
/// @endcond
#endif
}

#endif // __WIL_ALLOCATORS_INCLUDED
