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
//! A family of types representing Win32 paths.
//! The API of these types are similar to that of `std::filesystem::path` with a few key differences:
//!     1.  This introduces non-owning "span" and "view" types for operations that don't need separate allocations
//!     2.  Includes support for both "wide" and "narrow" paths
//!     3.  Allows control of the underlying storage type
//!     4.  Allows "detaching" the underlying storage, e.g. for implementing an API that returns a path
//!     5.  Includes proper support of "long paths" (\\?\ prefixed paths)
#ifndef __WIL_PATH_INCLUDED
#define __WIL_PATH_INCLUDED

#include "result_macros.h"

#if __WI_LIBCPP_STD_VER < 17
#error The <wil/path.h> header requires C++17
#endif

#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
#include <string>
#endif

#if WIL_USE_STL && (__cpp_concepts >= 202002L)
#include <concepts>
#define __WIL_PATH_HAS_CONCEPTS 1
#endif

namespace wil
{
#if __WIL_PATH_HAS_CONCEPTS
//! Matches any type that behaves enough like a `std::basic_string_view<CharT>`.
//! Note that this is crafted such that it will match for both `std::basic_string` as well as `std::basic_string_view`,
//! however it won't match things like `std::array`, `std::vector`, or `std::span` as there's no guarantee that these
//! types were (1) intended to be used as strings, and (2) that the `size` of each only includes valid characters (this
//! can be particularly error prone for something like `std::array` used as a buffer where the string only consumes a
//! subset of the available space).
template <typename T, typename CharT>
concept StringViewLike = requires(const T str) {
    typename T::value_type;
    requires std::same_as<typename T::value_type, CharT>;
    { str[0] } -> std::same_as<const CharT&>;
    { str.front() } -> std::same_as<const CharT&>;
    { str.back() } -> std::same_as<const CharT&>;
    { str.data() } -> std::same_as<const CharT*>;
    { str.size() } -> std::same_as<size_t>;
    { str.length() } -> std::same_as<size_t>;
    { str.empty() } -> std::same_as<bool>;
};

//! Matches types that can be used for `basic_path`'s `Traits` template parameter.
//! This is mostly provided as documentation of the type's requirements, however it can also be used to more easily
//! diagnose build errors.
template <typename T>
concept PathTraits = requires {
    typename T::value_type; // `char` or `wchar_t`
    requires std::same_as<typename T::value_type, char> || std::same_as<typename T::value_type, wchar_t>;
    typename T::string_type; // E.g. `std::string`
    typename T::storage;     // E.g. `std::string` or some custom type that handles memory
    typename T::size_type;   // E.g. `size_t`
    typename T::result_type; // E.g. `void` or `HRESULT`
    { T::is_noexcept } -> std::convertible_to<bool>;

    // Operations on 'storage'
    requires requires(typename T::storage val, const typename T::storage constVal) {
        { T::data(val) } noexcept -> std::same_as<typename T::value_type*>;
        { T::data(constVal) } noexcept -> std::same_as<const typename T::value_type*>;
        { T::size(constVal) } noexcept -> std::same_as<typename T::size_type>;
    };
};
#else
/// @cond
namespace details
{
    // We need to SFINAE on `string_view`-like types, so we do our best to replicate the logic from above.
    template <typename T, typename CharT>
    struct is_string_view_like
    {
    private:
        template <typename U>
        static wistd::true_type eval(
            const U* str,
            wistd::enable_if_t<
                wistd::is_same_v<typename U::value_type, CharT> && wistd::is_same_v<decltype((*str)[0]), const CharT&> &&
                wistd::is_same_v<decltype(str->front()), const CharT&> && wistd::is_same_v<decltype(str->back()), const CharT&> &&
                wistd::is_same_v<decltype(str->data()), const CharT*> && wistd::is_same_v<decltype(str->size()), size_t> &&
                wistd::is_same_v<decltype(str->length()), size_t> && wistd::is_same_v<decltype(str->empty()), bool>>*);

        template <typename>
        static wistd::false_type eval(...);

    public:
        static constexpr const bool value = decltype(eval<T>(nullptr, nullptr))::value;
    };

    template <typename T, typename CharT>
    constexpr bool is_string_view_like_v = is_string_view_like<T, CharT>::value;

    // NOTE: We don't bother trying to create an equivalent for `PathTraits` since we don't need to SFINAE anything on
    // such types. Consumers pre-C++11 will just have to deal with the sub-optimal build failures if anything goes wrong
} // namespace details
/// @endcond
#endif

/// @cond
// Forward declarations
template <typename CharT>
struct basic_path_view;

template <typename CharT>
struct basic_path_span;

template <typename CharT, typename Traits>
struct basic_path;

// Traits types definitions
namespace details
{
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
    template <typename CharT, typename CharTraits, typename Alloc>
    struct basic_string_path_traits
    {
        using value_type = CharT;
        using string_type = std::basic_string<CharT, CharTraits, Alloc>;
        using size_type = typename string_type::size_type;
        using result_type = void; // Uses exceptions
        static constexpr bool is_noexcept = false;

        using storage = string_type; // std::basic_string handles its size/capacity

        static constexpr value_type* data(storage& str) noexcept
        {
            return str.data();
        }

        static constexpr const value_type* data(const storage& str) noexcept
        {
            return str.data();
        }

        static constexpr size_type size(const storage& str) noexcept
        {
            return str.size();
        }
    };
#endif

    template <typename CharT>
    struct char_traits;

    template <>
    struct char_traits<char>
    {
        static size_t length(const char* str) noexcept
        {
            return ::strlen(str);
        }
    };

    template <>
    struct char_traits<wchar_t>
    {
        static size_t length(const wchar_t* str) noexcept
        {
            return ::wcslen(str);
        }
    };

    // Since we don't want to take a dependency on std::allocator_traits being available, we implement what we need
    // here... This is a minimal subset of just what we need for correctness and is intentionally not feature complete.
    template <typename Alloc>
    struct allocator_traits
    {
    private:
        template <typename A, typename = decltype(wistd::declval<A>().select_on_container_copy_construction())>
        static wistd::true_type test_select_on_container_copy_construction(int);

        template <typename>
        static wistd::false_type test_select_on_container_copy_construction(...);

        template <typename A, typename = typename A::is_always_equal>
        static wistd::true_type test_is_always_equal(int);

        template <typename>
        static wistd::false_type test_is_always_equal(...);

        template <typename A, typename = typename A::propagate_on_container_copy_assignment>
        static wistd::true_type test_propagate_on_container_copy_assignment(int);

        template <typename>
        static wistd::false_type test_propagate_on_container_copy_assignment(...);

        template <typename A, typename = typename A::propagate_on_container_move_assignment>
        static wistd::true_type test_propagate_on_container_move_assignment(int);

        template <typename>
        static wistd::false_type test_propagate_on_container_move_assignment(...);

        template <typename A, typename = typename A::propagate_on_container_swap>
        static wistd::true_type test_propagate_on_container_swap(int);

        template <typename>
        static wistd::false_type test_propagate_on_container_swap(...);

        struct wrapper
        {
            using is_always_equal = wistd::is_empty<Alloc>;
            using propagate_on_container_copy_assignment = wistd::false_type;
            using propagate_on_container_move_assignment = wistd::false_type;
            using propagate_on_container_swap = wistd::false_type;
        };

    public:

        static constexpr Alloc select_on_container_copy_construction(const Alloc& alloc)
        {
            if constexpr (decltype(test_select_on_container_copy_construction<Alloc>(0))::value)
            {
                return alloc.select_on_container_copy_construction();
            }
            else
            {
                return alloc;
            }
        }

        using is_always_equal =
            typename wistd::conditional_t<decltype(test_is_always_equal<Alloc>(0))::value, Alloc, wrapper<Alloc>>::is_always_equal;
        using propagate_on_container_copy_assignment =
            typename wistd::conditional_t<decltype(test_propagate_on_container_copy_assignment<Alloc>(0))::value, Alloc, wrapper<Alloc>>::propagate_on_container_copy_assignment;
        using propagate_on_container_move_assignment =
            typename wistd::conditional_t<decltype(test_propagate_on_container_move_assignment<Alloc>(0))::value, Alloc, wrapper<Alloc>>::propagate_on_container_move_assignment;
        using propagate_on_container_swap =
            typename wistd::conditional_t<decltype(test_propagate_on_container_swap<Alloc>(0))::value, Alloc, wrapper<Alloc>>::propagate_on_container_swap;
    };

    template <typename CharT, typename UniqueStringT, typename Alloc, typename ErrPolicy>
    struct unique_path_traits
    {
        using value_type = CharT;
        using string_type = UniqueStringT;
        using size_type = typename Alloc::size_type;

        // NOTE: There's an implicit assumption that if 'ErrPolicy::is_noexcept' is true, 'Alloc' should not throw on
        // allocation failure.
        using result_type = typename ErrPolicy::result;
        static constexpr bool is_noexcept = ErrPolicy::is_noexcept;

        struct storage : private ebo<Alloc>
        {
        private:
            using AllocBase = ebo<Alloc>;
            using AllocTraits = allocator_traits<Alloc>;

            // std::allocator<T>::rebind was removed in C++20 and since we can't easily take a dependency on
            // std::allocator_traits we don't have an easy way to rebind the allocator... For now we take a dependency
            // on the allocator's `value_type` matching ours, which should be a reasonable requirement.
            static_assert(wisd::is_same_v<typename Alloc::value_type, value_type>, "Allocator's 'value_type' must match the trait's character type");

            // Avoid allocating just a few bytes since it's likely we'll want more. Note that we don't implement small
            // string optimization under the assumption that most paths will be longer and many use cases for shorter
            // paths (such as extension, filename, etc.), using other types like `basic_path_view` are more optimal.
            // FUTURE: Since we currently delay allocation for default-constructed paths, we may be able to get away
            // with implementing a small string optimization since we need to check for the empty case anyway.
            static constexpr const size_type minimum_capacity = 15; // I.e. allocate at least 16 characters-worth

            // Although the path object logically holds a 'UniqueStringT', we want to make default constructed path
            // objects efficient and light weight, which means not allocating unless we absolutely need to (e.g. if the
            // caller explicitly asks for a 'UniqueStringT' object). In order to do this we take advantage of the fact
            // that 'm_size' will be zero and can therefore double as a null terminator.
            value_type* m_data = reinterpret_cast<value_type*>(&m_size);
            size_type m_size = 0;
            size_type m_capacity = 0;

            template <wistd::enable_if_t<wistd::is_same_v<result_type, void>, int> = 0> // Sanity check
            storage(const value_type* str, size_type len, Alloc&& alloc) noexcept(is_noexcept) : AllocBase(wistd::move(alloc))
            {
                if (len == 0)
                {
                    return;
                }

                auto newCapacity = (wistd::max)(len, minimum_capacity);
                m_data = get_allocator().allocate(newCapacity + 1);
                if (!m_data)
                {
                    // Ideally the allocator would throw, but this is "just to be safe"
                    ErrPolicy::HResult(E_OUTOFMEMORY);
                }

                ::memcpy(m_data, str, len * sizeof(value_type));
                m_size = len;
                m_capacity = newCapacity;
                m_data[m_size] = '\0';
            }

        public:
            storage() noexcept = default;

            template <wistd::enable_if_t<wistd::is_same_v<result_type, void>, int> = 0>
            storage(const storage& other) noexcept(is_noexcept && wistd::is_nothrow_copy_constructible_v<Alloc>) :
                storage(other.m_data, other.m_size, AllocTraits::select_on_container_copy_construction(other.get_allocator()))
            {
            }

            storage(storage&& other) noexcept : AllocBase(wistd::move(other.get_allocator()))
            {
                // If other is not allocated, we don't want to take its buffer since it points inside other!
                if (other.is_allocated())
                {
                    m_data = other.m_data;
                    m_size = other.m_size;
                    m_capacity = other.m_capacity;
                    other.transition_to_default_state();
                }
            }

            template <wistd::enable_if_t<wistd::is_same_v<result_type, void>, int> = 0>
            storage(const value_type* str, size_type len, const Alloc& alloc = Alloc()) noexcept(
                is_noexcept && wistd::is_nothrow_copy_constructible_v<Alloc>) :
                storage(str, len, Alloc(alloc))
            {
            }

            ~storage()
            {
                destroy();
            }

            template <wistd::enable_if_t<wistd::is_same_v<result_type, void>, int> = 0>
            storage& operator=(const storage& other) noexcept(
                is_noexcept && (!AllocTraits::propagate_on_container_copy_assignment::value ||
                                AllocTraits::is_always_equal::value || wistd::is_nothrow_copy_assignable_v<Alloc>))
            {
                if (this != &other)
                {
                    if constexpr (AllocTraits::propagate_on_container_copy_assignment::value && !AllocTraits::is_always_equal::value)
                    {
                        if (get_allocator() != other.get_allocator())
                        {
                            // We're getting a new allocator... anything we've already allocated needs to get thrown away
                            destroy();
                            transition_to_default_state();

                            get_allocator() = other.get_allocator();
                        }
                    }

                    assign(other.m_data, other.m_size);
                }

                return *this;
            }

            storage& operator=(storage&& other) noexcept(
                is_noexcept || AllocTraits::propagate_on_container_move_assignment::value || AllocTraits::is_always_equal::value)
            {
                if constexpr (!AllocTraits::propagate_on_container_move_assignment::value)
                {
                    if constexpr (!AllocTraits::is_always_equal::value)
                    {
                        if (get_allocator() != other.get_allocator())
                        {
                            // Unfortunate scenario where we can't just steal memory from other
                            assign(other.m_data, other.m_size);
                            return;
                        }
                    }

                    if ((other.m_size == 0) && (other.m_capacity <= m_capacity))
                    {
                        // This is a small optimization - if other is empty, we can keep our current buffer (if it
                        // exists), which may elide a future allocation.
                        *m_data = '\0';
                        m_size = 0;
                        return;
                    }

                    destroy();
                }
                else
                {
                    // Since we're overwriting our allocator, we need to release any resources we've acquired first.
                    // This has the downside that if 'other' is empty and we have allocated memory, we'll be throwing
                    // away a perfectly good buffer for nothing. It's assumed that usage patterns would make this
                    // unlikely to occur.
                    destroy();
                    get_allocator() = wistd::move(other.get_allocator());
                }
                // NOTE: Assumption is that we're **NOT** in the default state at this point

                if (other.is_allocated())
                {
                    // We can steal other's memory
                    m_data = other.m_data;
                    m_size = other.m_size;
                    m_capacity = other.m_capacity;
                    other.transition_to_default_state();
                }
                else
                {
                    // All other code paths call 'destroy', but don't reset other values - do so now
                    transition_to_default_state();
                }

                return *this;
            }

            void swap(storage& other) noexcept(is_noexcept || AllocTraits::propagate_on_container_swap::value || AllocTraits::is_always_equal::value)
            {
                if constexpr (AllocTraits::propagate_on_container_swap::value)
                {
                    wistd::swap_wil(get_allocator(), other.get_allocator());
                }
                else if constexpr (!AllocTraits::is_always_equal::value)
                {
                    if (get_allocator() != other.get_allocator())
                    {
                        // Unfortunate scenario where we can't just swap memory pointers
                        auto temp = wistd::move(*this);
                        assign(other.m_data, other.m_size);
                        other.assign(temp.m_data, temp.m_size);
                        return;
                    }
                    // Otherwise it's "as if" we swapped the allocators
                }

                if (is_allocated())
                {
                    if (other.is_allocated())
                    {
                        // Easy case - both allocated, just swap pointers/sizes
                        wistd::swap_wil(m_data, other.m_data);
                        wistd::swap_wil(m_size, other.m_size);
                        wistd::swap_wil(m_capacity, other.m_capacity);
                    }
                    else
                    {
                        // This object has data, but other doesn't
                        other.m_data = m_data;
                        other.m_size = m_size;
                        other.m_capacity = m_capacity;
                        transition_to_default_state();
                    }
                }
                else if (other.is_allocated())
                {
                    // Other has data, but this object doesn't
                    m_data = other.m_data;
                    m_size = other.m_size;
                    m_capacity = other.m_capacity;
                    other.transition_to_default_state();
                }
                // Otherwise both empty - nothing to do
            }

            value_type* data() noexcept
            {
                return m_data;
            }

            const value_type* data() const noexcept
            {
                return m_data;
            }

            size_type size() const noexcept
            {
                return m_size;
            }

            result_type assign(const value_type* str, size_type len) noexcept(is_noexcept)
            {
                if (len > m_capacity)
                {
                    // NOTE: Assignment does not take the amortized doubling path
                    auto newCapacity = (wistd::max)(len, minimum_capacity);
                    auto newPtr = AllocBase::get().allocate(newCapacity + 1);
                    if (!newPtr)
                    {
                        return ErrPolicy::HResult(E_OUTOFMEMORY);
                    }

                    destroy();
                    m_data = newPtr;
                    m_capacity = newCapacity;
                    // m_size set below
                }

                ::memcpy(m_data, str, len * sizeof(value_type));
                m_size = len;
                m_data[m_size] = '\0';

                return ErrPolicy::OK();
            }

            result_type append(const value_type* str, size_type len) noexcept(is_noexcept)
            {
                auto newLen = m_size + len;
                if (newLen > m_capacity)
                {
                    auto newCapacity = (wistd::max)(newLen, m_capacity * 2);
                    newCapacity = (wistd::max)(newCapacity, minimum_capacity);
                    WI_ASSERT(newCapacity >= (m_size + len));

                    auto newPtr = AllocBase::get().allocate(newCapacity + 1);
                    if (!newPtr)
                    {
                        return ErrPolicy::HResult(E_OUTOFMEMORY);
                    }

                    ::memcpy(newPtr, m_data, m_size * sizeof(value_type));
                    // NOTE: Null terminator written below

                    destroy();
                    m_data = newPtr;
                    m_capacity = newCapacity;
                    // m_size is correct & updated below
                }

                ::memcpy(m_data + m_size, str, len * sizeof(value_type));
                m_size += len;
                m_data[m_size] = '\0';

                return ErrPolicy::OK();
            }

        private:

            bool is_allocated() const noexcept
            {
                WI_ASSERT((m_capacity == 0) == (m_data == reinterpret_cast<value_type*>(&m_size)));
                return m_capacity != 0;
            }

            Alloc& get_allocator() noexcept
            {
                return AllocBase::get();
            }

            const Alloc& get_allocator() const noexcept
            {
                return AllocBase::get();
            }

            void destroy() noexcept
            {
                if (!is_allocated())
                {
                    WI_ASSERT((m_size == 0) && (m_capacity == 0));
                    return; // Not allocated; nothing to do
                }

                get_allocator().deallocate(m_data, m_capacity + 1);
                // NOTE: It's the caller's responsibilty for setting pointers/values as appropriate. If default values
                // are desired, call 'transition_to_default_state'
            }

            void transition_to_default_state() noexcept
            {
                m_data = reinterpret_cast<value_type*>(&m_size);
                m_size = 0;
                m_capacity = 0;
            }
        };

        static constexpr value_type* data(storage& str) noexcept
        {
            return str.data();
        }

        static constexpr const value_type* data(const storage& str) noexcept
        {
            return str.data();
        }

        static constexpr size_type size(const storage& str) noexcept
        {
            return str.size();
        }
    };
}

// Implementation of common operations
namespace details
{
    template <typename CharT>
    struct path_subspan
    {
        CharT* data = nullptr;
        size_t size = 0;
    };

    template <typename CharT>
    constexpr bool is_letter(CharT ch) noexcept
    {
        return ((ch >= 'A') &&s (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z'));
    }

    template <typename CharT>
    constexpr bool is_separator(CharT ch) noexcept
    {
        return (ch == '\\') || (ch == '/');
    }

    template <typename CharT>
    constexpr size_t next_backslash(CharT* data, size_t size, size_t startIndex = 0) noexcept
    {
        for (size_t i = startIndex; i < size; ++i)
        {
            if (data[i] == '\\')
            {
                return i;
            }
        }

        return size;
    }

    template <typename CharT>
    constexpr size_t next_separator(CharT* data, size_t size, size_t startIndex = 0) noexcept
    {
        for (size_t i = startIndex; i < size; ++i)
        {
            if (is_separator(data[i]))
            {
                return i;
            }
        }

        return size;
    }

    template <typename CharT>
    constexpr size_t next_non_separator(CharT* data, size_t size, size_t startIndex = 0) noexcept
    {
        for (size_t i = startIndex; i < size; ++i)
        {
            if (!is_separator(data[i]))
            {
                return i;
            }
        }

        return size;
    }

    /*
     * For more info on 'root_name', see: https://en.cppreference.com/w/cpp/filesystem/path/root_name.html
     * Unlike 'std::filesystem::path', we recognize all of the following as root names.
     * IMPORTANT: Unlike 'std::filesystem::path', we include the share in the root name for UNC paths.
     *      X:
     *      \\server\share
     *      \\?\X:
     *      \\?\UNC\server\share
     */
    template <typename CharT>
    constexpr path_subspan<CharT> root_name(CharT* data, size_t size) noexcept
    {
        if (size < 2)
        {
            return {};
        }

        switch (data[0])
        {
        default:
            // Expect a drive letter
            if (is_letter(data[0]) && (data[1] == ':'))
            {
                return {data, 2}; // e.g. X:
            }

            return {};

        case '/':
            // Extended paths require backslashes, so expect a "normal" UNC path here of the form "//server/share"
            break; // Fall through to common UNC handling

        case '\\':
            // Expect either a UNC path or an extended path. This gets interesting because only "normal" UNC paths allow
            // the second character to be a forward slash.
            if (data[1] == '/')
            {
                break; // Fall through to common UNC handling
            }
            if (data[1] != '\\')
            {
                return {}; // Path rooted on current drive... no root name
            }

            if ((size < 3) || (data[2] != '?'))
            {
                break; // Can't be or isn't an extended path; fall through to check for a "normal" UNC path
            }

            // Shortest valid extended path is "\\?\X:"
            if ((size < 6) || data[3] != '\\')
            {
                return {}; // Invalid extended path
            }

            if (is_letter(data[4]) && (data[5] == ':'))
            {
                return {data, 6}; // e.g. \\?\X:
            }

            // Only other valid extended path is UNC of the form "\\?\UNC\server\share"
            if ((size < 9) || (data[4] != 'U') || (data[5] != 'N') || (data[6] != 'C') || (data[7] != '\\'))
            {
                return {}; // Invalid extended path
            }





            return {};
        }

        // If we fall through, we're handling a "normal" UNC path. The first two characters should be confirmed
        // NOTE: Although it looks weird, the second character does not need to match the first. I.e.
        // "/\server/share" is okay
        assert(is_separator(data[0]) && is_separator(data[1]));
        if (size < 3)
        {
            return {}; // We expect at least a server name
        }

        if (is_separator(data[2]))
        {
            return {}; // Three initial separators is an invalid UNC path
        }

        // We don't validate names; all we care about are the separators
        auto pos = next_separator(data, size, 3);
        if (pos == size)
        {
            return {data, size}; // Server name only with no share
        }

        // Multiple consecutive separators are treated as one
        pos = next_non_separator(data, size, pos + 1);
        if (pos == size)
        {
            // This is a bit of an odd situation... The input is something like "//server/" with a trailing separator,
            // but no share name. We consider that final separator to be part of the root name since it otherwise would
            // be if there was a share name.
            return {data, size};
        }

        return {data, next_separator(data, size, pos)};
    }

    template <typename CharT>
    constexpr path_subspan<CharT> root_directory(CharT* data, size_t size) noexcept
    {
        // TODO
    }

    template <typename CharT>
    constexpr path_subspan<CharT> root_path(CharT* data, size_t size) noexcept
    {
        // TODO
    }

    template <typename CharT>
    constexpr path_subspan<CharT> relative_path(CharT* data, size_t size) noexcept
    {
        // TODO
    }

    template <typename CharT>
    constexpr path_subspan<CharT> parent_path(CharT* data, size_t size) noexcept
    {
        // TODO
    }

    template <typename CharT>
    constexpr path_subspan<CharT> filename(CharT* data, size_t size) noexcept
    {
        // TODO
    }

    template <typename CharT>
    constexpr path_subspan<CharT> stem(CharT* data, size_t size) noexcept
    {
        // TODO
    }

    template <typename CharT>
    constexpr path_subspan<CharT> extension(CharT* data, size_t size) noexcept
    {
        // TODO
    }

    template <typename CharT>
    constexpr path_subspan<CharT> remove_filename(CharT* data, size_t size) noexcept
    {
        // TODO
    }
}
/// @endcond
}

#endif
