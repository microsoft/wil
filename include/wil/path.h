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
            return strlen(str);
        }
    };

    template <>
    struct char_traits<wchar_t>
    {
        static size_t length(const wchar_t* str) noexcept
        {
            return wcslen(str);
        }
    };

    template <typename CharT, typename UniqueStringT, typename Alloc, typename ErrPolicy>
    struct unique_path_traits
    {
        using value_type = CharT;
        using string_type = UniqueStringT;
        using size_type = typename Alloc::size_type;
        using result_type = typename ErrPolicy::result;
        static constexpr bool is_noexcept = ErrPolicy::is_noexcept;

        struct storage : private ebo<Alloc>
        {
        private:
            using AllocBase = ebo<Alloc>;

            // std::allocator<T>::rebind was removed in C++20 and since we can't easily take a dependency on
            // std::allocator_traits we don't have an easy way to rebind the allocator... For now we take a dependency
            // on the allocator's `value_type` matching ours
            static_assert(wisd::is_same_v<typename Alloc::value_type, value_type>, "Allocator's 'value_type' must match the trait's character type");

            // Avoid allocating just a few bytes since it's likely we'll want more
            static constexpr const size_type minimum_capacity = 15; // I.e. allocate at least 16 characters-worth

            // Although the path object logically holds a 'UniqueStringT', we want to make default constructed path
            // objects efficient and light weight, which means not allocating unless we absolutely need to (e.g. if the
            // caller explicitly asks for a 'UniqueStringT' object). In order to do this we take advantage of the fact
            // that 'm_size' will be zero and can therefore double as a null terminator.
            value_type* m_data = reinterpret_cast<value_type*>(&m_size);
            size_type m_size = 0;
            size_type m_capacity = 0;

        public:
            storage() noexcept = default;

            template <wistd::enable_if_t<wistd::is_same_v<result_type, void>, int> = 0>
            storage(const storage& other) noexcept(is_noexcept) : storage(other.m_data, other.m_size)
            {
            }

            storage(storage&& other) noexcept
            {
                // If other is not allocated, we don't want to take its buffer since it points inside other!
                if (other.is_allocated())
                {
                    m_data = other.m_data;
                    other.m_data = reinterpret_cast<value_type*>(&other.m_size);
                    m_size = wistd::exchange(other.m_size, 0);
                    m_capacity = wistd::exchange(other.m_capacity, 0);
                }
            }

            template <wistd::enable_if_t<wistd::is_same_v<result_type, void>, int> = 0>
            storage(const value_type* str, size_type len) noexcept(is_noexcept)
            {
                if (len == 0)
                {
                    return;
                }

                auto newCapacity = (wistd::max)(len, minimum_capacity);
                m_data = AllocBase::get().allocate(newCapacity + 1);
                if (!m_data)
                {
                    ErrPolicy::HResult(E_OUTOFMEMORY);
                }

                memcpy(m_data, str, len * sizeof(value_type));
                m_size = len;
                m_capacity = newCapacity;
                m_data[m_size] = '\0';
            }

            ~storage()
            {
                destroy();
            }

            template <wistd::enable_if_t<wistd::is_same_v<result_type, void>, int> = 0>
            storage& operator=(const storage& other) noexcept(is_noexcept)
            {
                if (this != &other)
                {
                    assign(other.m_data, other.m_size);
                }

                return *this;
            }

            storage& operator=(storage&& other) noexcept
            {
                // TODO
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

                memcpy(m_data, str, len * sizeof(value_type));
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
                    auto newPtr = AllocBase::get().allocate(newCapacity + 1);
                    if (!newPtr)
                    {
                        return ErrPolicy::HResult(E_OUTOFMEMORY);
                    }

                    memcpy(newPtr, m_data, m_size * sizeof(value_type));

                    destroy();
                    m_data = newPtr;
                    m_capacity = newCapacity;
                    // m_size set below
                }

                memcpy(m_data + m_size, str, len * sizeof(value_type));
                m_size += len;
                m_data[m_size] = '\0';

                return ErrPolicy::OK();
            }

        private:

            bool is_allocated() const noexcept
            {
                return m_data != reinterpret_cast<value_type*>(&m_size);
            }

            void destroy() noexcept
            {
                if (!is_allocated())
                {
                    WI_ASSERT((m_size == 0) && (m_capacity == 0));
                    return; // Not allocated; nothing to do
                }

                // NOTE: It's the caller's responsibilty for setting pointers/values as appropriate
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
    /*
     * We recognize the following as root paths:
     *      
     */
    template <typename PathT>
    constexpr PathT root_name(const PathT& path) noexcept
    {
        // 
        // TODO
    }

    template <typename PathT>
    constexpr PathT root_directory(const PathT& path) noexcept
    {
        // TODO
    }

    template <typename PathT>
    constexpr PathT root_path(const PathT& path) noexcept
    {
        // TODO
    }

    template <typename PathT>
    constexpr PathT relative_path(const PathT& path) noexcept
    {
        // TODO
    }

    template <typename PathT>
    constexpr PathT parent_path(const PathT& path) noexcept
    {
        // TODO
    }

    template <typename PathT>
    constexpr PathT filename(const PathT& path) noexcept
    {
        // TODO
    }

    template <typename PathT>
    constexpr PathT stem(const PathT& path) noexcept
    {
        // TODO
    }

    template <typename PathT>
    constexpr PathT extension(const PathT& path) noexcept
    {
        // TODO
    }

    template <typename PathT>
    constexpr PathT remove_filename(const PathT& path) noexcept
    {
        // TODO
    }
}
/// @endcond
}

#endif
