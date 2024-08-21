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
//! Win32 specific path types
#ifndef __WIL_PATH_INCLUDED
#define __WIL_PATH_INCLUDED

#include <stdint.h>
#include <wchar.h>

#include "result_macros.h"

namespace wil
{
//! Represents a non-modifiable view of a Win32 filesystem path or path segment. This type behaves like a combination of
//! `std::basic_string_view` and `std::filesystem::path`. The underlying string is not necessarily null-terminated and therefore
//! cannot easily be passed to OS APIs. It is most useful as an intermediate type when performing multiple operations on a single
//! path or when null-termination is not needed, such as when constructing a longer path, comparing file extensions, etc. Proper
//! use of this type allows significantly more efficient operations when compared to something like `std::filesystem::path`, which
//! must allocate a new string for each intermediate operation.
//! @tparam err_policy      Represents the error policy for the class (error codes, exceptions, or fail fast; see @ref page_errors)
template <typename err_policy = err_exception_policy>
class path_view_t;

template <typename err_policy = err_exception_policy>
class path_ref_t;

template <typename err_policy = err_exception_policy>
class path;

/// @cond
namespace details
{
    constexpr const size_t npos = static_cast<size_t>(0) - 1;

    struct character_bitmask
    {
        bool set(const wchar_t* chars, size_t count) noexcept
        {
            for (size_t i = 0; i < count; ++i)
            {
                auto ch = chars[i];
                if (ch >= 256)
                {
                    return false;
                }

                auto index = ch / 64;
                auto bit = ch % 64;
                bitmasks[index] |= 0x01 << bit;
            }

            return true;
        }

        bool is_set(wchar_t ch) const noexcept
        {
            if (ch >= 256)
            {
                return false;
            }

            auto index = ch / 64;
            auto bit = ch % 64;
            return (bitmasks[index] & (0x01 << bit)) != 0;
        }

    private:

        uint64_t bitmasks[4] = {}; // 0-255
    };

    template <typename WCharT, typename err_policy>
    struct path_base
    {
        using result_type = typename err_policy::result;

        // NOTE: This kind of gets messy with the implementation of functions, however the non-constness of the various types is
        // done to match that of std::basic_string_view
        using value_type = wistd::remove_const_t<WCharT>;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        // TODO: Iterators
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        static inline constexpr const size_type npos = details::npos;

        constexpr WCharT& operator[](size_type index) noexcept
        {
            WI_ASSERT(index < m_length);
            return m_data[index];
        }

        constexpr const_reference operator[](size_type index) const noexcept
        {
            WI_ASSERT(index < m_length);
            return m_data[index];
        }

        template <typename ErrPolicy = err_policy>
        constexpr WCharT& at(size_type index) noexcept(err_policy_traits<ErrPolicy>::is_nothrow)
        {
            static_assert(wistd::is_same_v<typename ErrPolicy::result, void>, "at requires exceptions or fail fast; use operator[] instead");

            if (index >= m_length)
            {
                ErrPolicy::HResult(E_BOUNDS);
            }

            return m_data[index];
        }

        template <typename ErrPolicy = err_policy>
        constexpr const_reference at(size_type index) const noexcept(err_policy_traits<ErrPolicy>::is_nothrow)
        {
            static_assert(wistd::is_same_v<typename ErrPolicy::result, void>, "at requires exceptions or fail fast; use operator[] instead");

            if (index >= m_length)
            {
                ErrPolicy::HResult(E_BOUNDS);
            }

            return m_data[index];
        }

        constexpr WCharT& front() noexcept
        {
            WI_ASSERT(!empty());
            return m_data[0];
        }

        constexpr const_reference front() const noexcept
        {
            WI_ASSERT(!empty());
            return m_data[0];
        }

        constexpr WCharT& back() noexcept
        {
            WI_ASSERT(!empty());
            return m_data[m_length - 1];
        }

        constexpr const_reference back() const noexcept
        {
            WI_ASSERT(!empty());
            return m_data[m_length - 1];
        }

        constexpr WCharT* data() noexcept
        {
            return m_data;
        }

        constexpr const_pointer data() const noexcept
        {
            return m_data;
        }

        constexpr size_type size() const noexcept
        {
            return m_length;
        }

        constexpr size_type length() const noexcept
        {
            return m_length;
        }

        constexpr size_type max_size() const noexcept
        {
            return details::npos / sizeof(WCharT);
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return m_length == 0;
        }

        // NOTE: remove_prefix, remove_suffix, and substr intentionally left out since that might require memory management

        constexpr void swap(path_base& other) noexcept
        {
            wistd::swap_wil(m_data, other.m_data);
            wistd::swap_wil(m_length, other.m_length);
        }

        // FUTURE: Can be made 'constexpr' by deducing presence of __builtin_memcpy or future MSVC equivalent, though there's no
        // significant point in doing so as this is more of a runtime type
        /*constexpr*/ size_type copy(wchar_t* dest, size_type count, size_type pos = 0) const noexcept
        {
            if (pos > m_length)
            {
                // NOTE: Differs from std::basic_string_view in that we don't throw/fail in this case
                return 0;
            }

            auto len = (wistd::min)(count, m_length - pos);
            ::memcpy(dest, m_data + pos, len * sizeof(wchar_t));
            return len;
        }

        // FUTURE: This does not include all of the overloads std::basic_string_view includes, but should be good enough for most
        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ int compare(StringViewLike&& other) const noexcept
        {
            auto otherLen = other.length();
            auto len = (wistd::min)(m_length, otherLen);
            auto result = ::wmemcmp(m_data, other.data(), len);
            if (result != 0)
            {
                return result;
            }

            if (m_length < otherLen)
            {
                return -1;
            }
            else if (m_length > otherLen)
            {
                return 1;
            }

            return 0;
        }

        /*constexpr*/ int compare(const wchar_t* str) const noexcept
        {
            // str may not actually be a path, but we don't do any validation and this properly handles null
            return compare(path_base<wistd::add_const_t<WCharT>, err_policy>(str));
        }

        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ bool starts_with(StringViewLike&& other) const noexcept
        {
            auto otherLen = other.length();
            if (otherLen > m_length)
            {
                return false;
            }

            return ::wmemcmp(m_data, other.data(), otherLen) == 0;
        }

        /*constexpr*/ bool starts_with(const wchar_t* str) const noexcept
        {
            // str may not actually be a path, but we don't do any validation and this properly handles null
            return starts_with(path_base<wistd::add_const_t<WCharT>, err_policy>(str));
        }

        constexpr bool starts_with(wchar_t ch) const noexcept
        {
            return empty() ? false : (m_data[0] == ch);
        }

        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ bool ends_with(StringViewLike&& other) const noexcept
        {
            auto otherLen = other.length();
            if (otherLen > m_length)
            {
                return false;
            }

            return ::wmemcmp(m_data + (m_length - otherLen), other.data(), otherLen) == 0;
        }

        /*constexpr*/ bool ends_with(const wchar_t* str) const noexcept
        {
            // str may not actually be a path, but we don't do any validation and this properly handles null
            return ends_with(path_base<wistd::add_const_t<WCharT>, err_policy>(str));
        }

        constexpr bool ends_with(wchar_t ch) const noexcept
        {
            return empty() ? false : (m_data[m_length - 1] == ch);
        }

        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ size_type find(StringViewLike&& other, size_type pos = 0) const noexcept
        {
            if (pos > m_length)
            {
                return npos;
            }

            auto searchLen = m_length - pos;
            auto otherLen = other.length();
            if (otherLen > searchLen)
            {
                return npos;
            }
            else if (otherLen == 0)
            {
                return pos; // Always finds empty string
            }

            auto searchPtr = m_data + pos;
            auto searchEnd = searchPtr + (searchLen - otherLen + 1); // +1 because this is treated like an end iterator
            auto otherPtr = other.data();
            while (true)
            {
                auto ptr = ::wmemchr(searchPtr, otherPtr[0], searchEnd - searchPtr);
                if (!ptr)
                {
                    return npos;
                }

                if (::wmemcmp(ptr, otherPtr, otherLen) == 0)
                {
                    return ptr - m_data;
                }

                // Not a full match; keep going
                searchPtr = ptr + 1;
            }
        }

        /*constexpr*/ size_type find(const wchar_t* str, size_type pos = 0) const noexcept
        {
            // str may not actually be a path, but we don't do any validation and this properly handles null
            return find(path_base<wistd::add_const_t<WCharT>, err_policy>(str), pos);
        }

        /*constexpr*/ size_type find(wchar_t ch, size_type pos = 0) const noexcept
        {
            if (pos >= m_length)
            {
                return npos;
            }

            auto ptr = ::wmemchr(m_data + pos, ch, m_length - pos);
            return ptr ? (ptr - m_data) : npos;
        }

        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ bool contains(StringViewLike&& other) const noexcept
        {
            return find(other) != npos;
        }

        /*constexpr*/ bool contains(const wchar_t* str) const noexcept
        {
            return find(str) != npos;
        }

        /*constexpr*/ bool contains(wchar_t ch) const noexcept
        {
            return find(ch) != npos;
        }

        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ size_type rfind(StringViewLike&& other, size_type pos = npos) const noexcept
        {
            auto otherLen = other.length();
            if (otherLen > m_length)
            {
                return npos;
            }

            pos = (wistd::min)(pos, m_length - otherLen);
            if (otherLen == 0)
            {
                return pos; // Always finds empty string
            }

            auto searchPtr = m_data + pos;
            auto otherPtr = other.data();
            while (true)
            {
                if (::wmemcmp(searchPtr, otherPtr, otherLen) == 0)
                {
                    return searchPtr - m_data;
                }

                if (searchPtr-- == m_data)
                {
                    return npos;
                }
            }
        }

        /*constexpr*/ size_type rfind(const wchar_t* str, size_type pos = npos) const noexcept
        {
            // str may not actually be a path, but we don't do any validation and this properly handles null
            return rfind(path_base<wistd::add_const_t<WCharT>, err_policy>(str), pos);
        }

        /*constexpr*/ size_type rfind(wchar_t ch, size_type pos = npos) const noexcept
        {
            if (empty())
            {
                return npos;
            }

            pos = (wistd::min)(pos, m_length - 1);
            while (true)
            {
                if (m_data[pos] == ch)
                {
                    return pos;
                }

                if (pos-- == 0)
                {
                    return npos;
                }
            }
        }

        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ size_type find_first_of(StringViewLike&& other, size_type pos = 0) const noexcept
        {
            auto otherData = other.data();
            auto otherLen = other.length();
            details::character_bitmask mask;
            if (mask.set(otherData, otherLen))
            {
                for (; pos < m_length; ++pos)
                {
                    if (mask.is_set(m_data[pos]))
                    {
                        return pos;
                    }
                }
            }
            else
            {
                for (; pos < m_length; ++pos)
                {
                    auto ch = m_data[pos];
                    for (size_t i = 0; i < otherLen; ++i)
                    {
                        if (ch == otherData[i])
                        {
                            return pos;
                        }
                    }
                }
            }

            return npos;
        }

        /*constexpr*/ size_type find_first_of(const wchar_t* str, size_type pos = 0) const noexcept
        {
            // str may not actually be a path, but we don't do any validation and this properly handles null
            return find_first_of(path_base<wistd::add_const_t<WCharT>, err_policy>(str), pos);
        }

        /*constexpr*/ size_type find_first_of(wchar_t ch, size_type pos = 0) const noexcept
        {
            return find(ch, pos);
        }

        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ size_type find_last_of(StringViewLike&& other, size_type pos = npos) const noexcept
        {
            if (empty())
            {
                return npos;
            }

            auto otherData = other.data();
            auto otherLen = other.length();
            pos = (wistd::min)(pos, m_length - 1);
            details::character_bitmask mask;
            if (mask.set(otherData, otherLen))
            {
                while (true)
                {
                    if (mask.is_set(m_data[pos]))
                    {
                        return pos;
                    }

                    if (pos-- == 0)
                    {
                        return npos;
                    }
                }
            }
            else
            {
                while (true)
                {
                    auto ch = m_data[pos];
                    for (size_t i = 0; i < otherLen; ++i)
                    {
                        if (ch == otherData[i])
                        {
                            return pos;
                        }
                    }

                    if (pos-- == 0)
                    {
                        return npos;
                    }
                }
            }
        }

        /*constexpr*/ size_type find_last_of(const wchar_t* str, size_type pos = npos) const noexcept
        {
            // str may not actually be a path, but we don't do any validation and this properly handles null
            return find_last_of(path_base<wistd::add_const_t<WCharT>, err_policy>(str), pos);
        }

        /*constexpr*/ size_type find_last_of(wchar_t ch, size_type pos = npos) const noexcept
        {
            return rfind(ch, pos);
        }

        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ size_type find_first_not_of(StringViewLike&& other, size_type pos = 0) const noexcept
        {
            auto otherData = other.data();
            auto otherLen = other.length();
            details::character_bitmask mask;
            if (mask.set(otherData, otherLen))
            {
                for (; pos < m_length; ++pos)
                {
                    if (!mask.is_set(m_data[pos]))
                    {
                        return pos;
                    }
                }
            }
            else
            {
                for (; pos < m_length; ++pos)
                {
                    auto ch = m_data[pos];
                    for (size_t i = 0; i < otherLen; ++i)
                    {
                        if (ch != otherData[i])
                        {
                            return pos;
                        }
                    }
                }
            }

            return npos;
        }

        /*constexpr*/ size_type find_first_not_of(const wchar_t* str, size_type pos = 0) const noexcept
        {
            // str may not actually be a path, but we don't do any validation and this properly handles null
            return find_first_not_of(path_base<wistd::add_const_t<WCharT>, err_policy>(str), pos);
        }

        /*constexpr*/ size_type find_first_not_of(wchar_t ch, size_type pos = 0) const noexcept
        {
            // ch is pretty clearly not a path, but we don't do any validation
            return find_first_not_of(path_base<wistd::add_const_t<WCharT>, err_policy>(&ch, 1), pos);
        }

        template <typename StringViewLike, wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
        /*constexpr*/ size_type find_last_not_of(StringViewLike&& other, size_type pos = npos) const noexcept
        {
            if (empty())
            {
                return npos;
            }

            auto otherData = other.data();
            auto otherLen = other.length();
            pos = (wistd::min)(pos, m_length - 1);
            details::character_bitmask mask;
            if (mask.set(otherData, otherLen))
            {
                while (true)
                {
                    if (!mask.is_set(m_data[pos]))
                    {
                        return pos;
                    }

                    if (pos-- == 0)
                    {
                        return npos;
                    }
                }
            }
            else
            {
                while (true)
                {
                    auto ch = m_data[pos];
                    for (size_t i = 0; i < otherLen; ++i)
                    {
                        if (ch != otherData[i])
                        {
                            return pos;
                        }
                    }

                    if (pos-- == 0)
                    {
                        return npos;
                    }
                }
            }
        }

        /*constexpr*/ size_type find_last_not_of(const wchar_t* str, size_type pos = npos) const noexcept
        {
            // str may not actually be a path, but we don't do any validation and this properly handles null
            return find_last_not_of(path_base<wistd::add_const_t<WCharT>, err_policy>(str), pos);
        }

        /*constexpr*/ size_type find_last_not_of(wchar_t ch, size_type pos = npos) const noexcept
        {
            // ch is pretty clearly not a path, but we don't do any validation
            return find_last_not_of(path_base<wistd::add_const_t<WCharT>, err_policy>(&ch, 1), pos);
        }

        // Path functions

    protected:

        constexpr path_base() noexcept = default;

        constexpr path_base(WCharT* data, size_type length) noexcept : m_data(data), m_length(length)
        {
        }

        /*constexpr*/ path_base(WCharT* data) noexcept : m_data(data), m_length(data ? ::wcslen(data) : 0)
        {
        }

        // NOTE: The derived type is responsible for managing the lifetime of this pointer, as applicable
        WCharT* m_data = nullptr;
        size_type m_length = 0;
    };
}
/// @endcond

template <typename err_policy>
class path_view_t : public details::path_base<const wchar_t, err_policy>
{
    using BaseT = details::path_base<const wchar_t, err_policy>;

public:
    using result_type = typename BaseT::result_type;

    using value_type = typename BaseT::value_type;
    using pointer = typename BaseT::pointer;
    using const_pointer = typename BaseT::const_pointer;
    using reference = typename BaseT::reference;
    using const_reference = typename BaseT::const_reference;
    // TODO: Iterators
    using size_type = typename BaseT::size_type;
    using difference_type = typename BaseT::difference_type;

    static inline constexpr const size_type npos = BaseT::npos;

    // This is a non-modifying view which need not be null-terminated, so a null pointer with length zero is valid
    constexpr path_view_t() noexcept = default;

    constexpr path_view_t(wistd::nullptr_t) noexcept
    {
    }

    constexpr path_view_t(const_pointer data, size_type length) noexcept : BaseT(data, length)
    {
    }

    /*constexpr*/ path_view_t(const_pointer data) noexcept : BaseT(data)
    {
    }

    // std::wstring_view overload (allows r-values)
    template <
        typename StringViewLike,
        wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t> && !details::is_string_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
    constexpr path_view_t(StringViewLike&& str) noexcept : BaseT(str.data(), str.length())
    {
    }

    // std::wstring overload (disallows r-values, i.e. temporaries)
    template <typename StringLike, wistd::enable_if_t<details::is_string_like<wistd::remove_reference_t<StringLike>, wchar_t>, int> = 0>
    constexpr path_view_t(StringLike& str) noexcept : BaseT(str.data(), str.length())
    {
    }

    // TODO: Iterators

    //! Constructs a `T` using the pointer and length combo. Used to easily convert between the various path types or to STL types
    //! such as `std::wstring` or `std::wstring_view`
    template <typename T>
    constexpr T as() const noexcept(wistd::is_nothrow_constructible_v<T, pointer, size_type>)
    {
        return T(BaseT::m_data, BaseT::m_length);
    }

    constexpr void remove_prefix(size_type count) noexcept
    {
        WI_ASSERT(count <= BaseT::m_length);
        BaseT::m_data += count;
        BaseT::m_length -= count;
    }

    constexpr void remove_suffix(size_type count) noexcept
    {
        WI_ASSERT(count <= BaseT::m_length);
        BaseT::m_length -= count;
    }

    constexpr path_view_t substr(size_type pos = 0, size_type count = npos) const noexcept
    {
        if (pos > BaseT::m_length)
        {
            // NOTE: Differs from std::basic_string_view in that we don't throw/fail in this case
            return path_view_t{};
        }

        auto len = (wistd::min)(count, BaseT::m_length - pos);
        return path_view_t(BaseT::m_data + pos, len);
    }
};

template <typename ErrPolicy>
inline /*constexpr*/ bool operator==(path_view_t<ErrPolicy> lhs, wistd::type_identity_t<path_view_t<ErrPolicy>> rhs) noexcept
{
    return lhs.compare(rhs) == 0;
}

template <typename ErrPolicy>
inline /*constexpr*/ bool operator!=(path_view_t<ErrPolicy> lhs, wistd::type_identity_t<path_view_t<ErrPolicy>> rhs) noexcept
{
    return lhs.compare(rhs) != 0;
}

template <typename ErrPolicy>
inline /*constexpr*/ bool operator<(path_view_t<ErrPolicy> lhs, wistd::type_identity_t<path_view_t<ErrPolicy>> rhs) noexcept
{
    return lhs.compare(rhs) < 0;
}

template <typename ErrPolicy>
inline /*constexpr*/ bool operator<=(path_view_t<ErrPolicy> lhs, wistd::type_identity_t<path_view_t<ErrPolicy>> rhs) noexcept
{
    return lhs.compare(rhs) <= 0;
}

template <typename ErrPolicy>
inline /*constexpr*/ bool operator>(path_view_t<ErrPolicy> lhs, wistd::type_identity_t<path_view_t<ErrPolicy>> rhs) noexcept
{
    return lhs.compare(rhs) > 0;
}

template <typename ErrPolicy>
inline /*constexpr*/ bool operator>=(path_view_t<ErrPolicy> lhs, wistd::type_identity_t<path_view_t<ErrPolicy>> rhs) noexcept
{
    return lhs.compare(rhs) >= 0;
}

using path_view = path_view_t<err_exception_policy>;
using path_view_nothrow = path_view_t<err_returncode_policy>;
using path_view_failfast = path_view_t<err_failfast_policy>;

template <typename err_policy>
class path_ref_t : public details::path_base<wchar_t, err_policy>
{
    using BaseT = details::path_base<const wchar_t, err_policy>;

public:
    using result_type = typename BaseT::result_type;

    using value_type = typename BaseT::value_type;
    using pointer = typename BaseT::pointer;
    using const_pointer = typename BaseT::const_pointer;
    using reference = typename BaseT::reference;
    using const_reference = typename BaseT::const_reference;
    // TODO: Iterators
    using size_type = typename BaseT::size_type;
    using difference_type = typename BaseT::difference_type;

    static inline constexpr const size_type npos = BaseT::npos;

    // This is a non-modifying view which need not be null-terminated, so a null pointer with length zero is valid
    constexpr path_ref_t() noexcept = default;

    constexpr path_ref_t(wistd::nullptr_t) noexcept
    {
    }

    constexpr path_ref_t(const_pointer data, size_type length) noexcept : BaseT(data, length)
    {
    }

    /*constexpr*/ path_ref_t(const_pointer data) noexcept : BaseT(data)
    {
    }

    // std::wstring_view overload (allows r-values)
    template <
        typename StringViewLike,
        wistd::enable_if_t<details::is_string_view_like<wistd::remove_reference_t<StringViewLike>, wchar_t> && !details::is_string_like<wistd::remove_reference_t<StringViewLike>, wchar_t>, int> = 0>
    constexpr path_ref_t(StringViewLike&& str) noexcept : BaseT(str.data(), str.length())
    {
    }

    // std::wstring overload (disallows r-values, i.e. temporaries)
    template <typename StringLike, wistd::enable_if_t<details::is_string_like<wistd::remove_reference_t<StringLike>, wchar_t>, int> = 0>
    constexpr path_ref_t(StringLike& str) noexcept : BaseT(str.data(), str.length())
    {
    }

    // TODO: Iterators

    //! Constructs a `T` using the pointer and length combo. Used to easily convert between the various path types or to STL types
    //! such as `std::wstring` or `std::wstring_view`
    template <typename T>
    constexpr T as() const noexcept(wistd::is_nothrow_constructible_v<T, pointer, size_type>)
    {
        return T(BaseT::m_data, BaseT::m_length);
    }

    constexpr void remove_prefix(size_type count) noexcept
    {
        WI_ASSERT(count <= BaseT::m_length);
        BaseT::m_data += count;
        BaseT::m_length -= count;
    }

    constexpr void remove_suffix(size_type count) noexcept
    {
        WI_ASSERT(count <= BaseT::m_length);
        BaseT::m_length -= count;
    }

    constexpr path_ref_t substr(size_type pos = 0, size_type count = npos) const noexcept
    {
        if (pos > BaseT::m_length)
        {
            // NOTE: Differs from std::basic_string_view in that we don't throw/fail in this case
            return path_ref_t{};
        }

        auto len = (wistd::min)(count, BaseT::m_length - pos);
        return path_ref_t(BaseT::m_data + pos, len);
    }
};

using path_ref = path_ref_t<err_exception_policy>;
using path_ref_nothrow = path_ref_t<err_returncode_policy>;
using path_ref_failfast = path_ref_t<err_failfast_policy>;
}

#endif // __WIL_PATH_INCLUDED
