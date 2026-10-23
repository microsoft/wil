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
//! Windows STL helpers: custom allocators for STL containers
#ifndef __WIL_STL_INCLUDED
#define __WIL_STL_INCLUDED

#include "common.h"
#include "resource.h"
#include <memory>
#include <string>
#include <vector>
#include <utility>
#if (__WI_LIBCPP_STD_VER >= 17) && WI_HAS_INCLUDE(<string_view>, 1) // Assume present if C++17
#include <string_view>
#endif

/// @cond
#ifndef WI_STL_FAIL_FAST_IF
#define WI_STL_FAIL_FAST_IF FAIL_FAST_IF
#endif
/// @endcond

#if defined(WIL_ENABLE_EXCEPTIONS)

namespace wil
{
/** Secure allocator for STL containers.
The `wil::secure_allocator` allocator calls `SecureZeroMemory` before deallocating
memory. This provides a mechanism for secure STL containers such as `wil::secure_vector`,
`wil::secure_string`, and `wil::secure_wstring`. */
template <typename T>
struct secure_allocator : public std::allocator<T>
{
    template <typename Other>
    struct rebind
    {
        using other = secure_allocator<Other>;
    };

    secure_allocator() : std::allocator<T>()
    {
    }

    ~secure_allocator() = default;

    secure_allocator(const secure_allocator& a) : std::allocator<T>(a)
    {
    }

    template <class U>
    secure_allocator(const secure_allocator<U>& a) : std::allocator<T>(a)
    {
    }

    T* allocate(size_t n)
    {
        return std::allocator<T>::allocate(n);
    }

    void deallocate(T* p, size_t n)
    {
        SecureZeroMemory(p, sizeof(T) * n);
        std::allocator<T>::deallocate(p, n);
    }
};

//! `wil::secure_vector` will be securely zeroed before deallocation.
template <typename Type>
using secure_vector = std::vector<Type, secure_allocator<Type>>;
//! `wil::secure_wstring` will be securely zeroed before deallocation.
using secure_wstring = std::basic_string<wchar_t, std::char_traits<wchar_t>, wil::secure_allocator<wchar_t>>;
//! `wil::secure_string` will be securely zeroed before deallocation.
using secure_string = std::basic_string<char, std::char_traits<char>, wil::secure_allocator<char>>;

/// @cond
namespace details
{
    template <>
    struct string_maker<std::wstring>
    {
        HRESULT make(_In_reads_opt_(length) PCWSTR source, size_t length) WI_NOEXCEPT
        try
        {
            m_value = source ? std::wstring(source, length) : std::wstring(length, L'\0');
            return S_OK;
        }
        catch (...)
        {
            return E_OUTOFMEMORY;
        }

        wchar_t* buffer()
        {
            return &m_value[0];
        }

        HRESULT trim_at_existing_null(size_t length)
        {
            m_value.erase(length);
            return S_OK;
        }

        std::wstring release()
        {
            return std::wstring(std::move(m_value));
        }

        static PCWSTR get(const std::wstring& value)
        {
            return value.c_str();
        }

    private:
        std::wstring m_value;
    };
} // namespace details
/// @endcond

// str_raw_ptr is an overloaded function that retrieves a const pointer to the first character in a string's buffer.
// This is the overload for std::wstring.  Other overloads available in resource.h.
inline PCWSTR str_raw_ptr(const std::wstring& str)
{
    return str.c_str();
}

#if __cpp_lib_string_view >= 201606L

#if defined(__WIL_OLEAUTO_H_)
// Create wil::unique_bstr from std::wstring_view (regardless if not null terminated, or if it contains embedded nulls)
inline wil::unique_bstr make_bstr_nothrow(std::wstring_view source) noexcept
{
    if (source.size() > 0xffffffffUL /*UINT32_MAX*/)
    {
        return wil::unique_bstr{};
    }
    return wil::unique_bstr(::SysAllocStringLen(source.data(), static_cast<UINT>(source.size())));
}
inline wil::unique_bstr make_bstr_failfast(std::wstring_view source) noexcept
{
    auto result(make_bstr_nothrow(source));
    FAIL_FAST_IF_NULL_ALLOC(result);
    return result;
}
#ifdef WIL_ENABLE_EXCEPTIONS
inline wil::unique_bstr make_bstr(std::wstring_view source)
{
    wil::unique_bstr result(make_bstr_nothrow(source));
    THROW_IF_NULL_ALLOC(result);
    return result;
}
#endif // WIL_ENABLE_EXCEPTIONS
#endif // defined(__WIL_OLEAUTO_H_)

template <class TChar, class Traits = std::char_traits<TChar>>
class basic_zstring_view;

/**
    Traits policy for a basic_zstring_view whose constructors reject null pointers and whose default constructor points
    at an internal empty string. The nested char_traits alias keeps the resulting basic_zstring_view derived from the
    same std::basic_string_view specialization as the nullable form.

*/
template <typename TChar, typename Traits = std::char_traits<TChar>>
struct nonnull_zstring_view_traits
{
    using char_traits = Traits;
    static constexpr bool empty_strings_are_non_null = true;
};

/// @cond
namespace details
{
    template <typename TChar, typename Traits, typename = void>
    struct zstring_view_traits
    {
        static constexpr bool empty_strings_are_non_null = false;
        using char_traits = Traits;
    };

    template <typename TChar, typename Traits>
    struct zstring_view_traits<TChar, Traits, std::void_t<decltype(Traits::empty_strings_are_non_null), typename Traits::char_traits>>
    {
        static constexpr bool empty_strings_are_non_null = Traits::empty_strings_are_non_null;
        using char_traits = typename Traits::char_traits;
    };

    template <typename T>
    struct is_basic_zstring_view : std::false_type
    {
    };

    template <typename TChar, typename Traits>
    struct is_basic_zstring_view<basic_zstring_view<TChar, Traits>> : std::true_type
    {
    };

    template <typename TChar>
    inline constexpr TChar zstring_view_empty_storage[1]{TChar()};
} // namespace details
/// @endcond

/**
    zstring_view. A zstring_view is identical to a std::string_view except it is always nul-terminated (unless empty).
    * zstring_view can be used for storing string literals without "forgetting" the length or that it is nul-terminated.
    * A zstring_view can be converted implicitly to a std::string_view because it is always safe to use a nul-terminated
      string_view as a plain string view.
    * A zstring_view can be constructed from a std::string because the data in std::string is nul-terminated.
    * substr(pos) returns a zstring_view because the tail remains nul-terminated. substr(pos, count) returns a
      std::string_view because an arbitrary slice may not be nul-terminated.
    * contains() is available before C++23 through a compatibility implementation.
    * nonnull_zstring_view uses a traits policy so its constructors produce non-null data(), including after default
      construction.

    @note basic_zstring_view publicly inherits from std::basic_string_view. A caller can explicitly cast any
    basic_zstring_view variant to a mutable base reference and assign data that is null or not nul-terminated. Avoid
    mutating the object through a base reference.
*/
template <class TChar, class Traits>
class basic_zstring_view : public std::basic_string_view<TChar, typename details::zstring_view_traits<TChar, Traits>::char_traits>
{
    using ZStringViewTraits = details::zstring_view_traits<TChar, Traits>;
    using BaseType = std::basic_string_view<TChar, typename ZStringViewTraits::char_traits>;
    using size_type = typename BaseType::size_type;

    template <class, class>
    friend class basic_zstring_view;

    template <typename T>
    struct has_c_str
    {
        template <typename U>
        static auto test(int) -> decltype(std::declval<U>().c_str(), std::true_type());
        template <typename U>
        static std::false_type test(...);
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    template <typename T>
    struct has_size
    {
        template <typename U>
        static auto test(int) -> decltype(std::declval<U>().size() == 1, std::true_type());
        template <typename U>
        static std::false_type test(...);
        static constexpr bool value = decltype(test<T>(0))::value;
    };

public:
    constexpr basic_zstring_view() noexcept : BaseType(default_view())
    {
    }
    constexpr basic_zstring_view(const basic_zstring_view&) noexcept = default;
    constexpr basic_zstring_view& operator=(const basic_zstring_view&) noexcept = default;

    constexpr basic_zstring_view(const TChar* pStringData, size_type stringLength) noexcept :
        BaseType(view_from_pointer(pStringData, stringLength))
    {
        validate_pointer_and_terminator();
    }

    template <size_t stringArrayLength>
    constexpr basic_zstring_view(const TChar (&stringArray)[stringArrayLength]) noexcept :
        BaseType(&stringArray[0], length_n(&stringArray[0], stringArrayLength))
    {
    }

    basic_zstring_view(std::nullptr_t) = delete;

    // Construct from nul-terminated char ptr. To prevent this from overshadowing array construction,
    // we disable this constructor if the value is an array (including string literal).
    template <typename TPtr, std::enable_if_t<std::is_convertible<TPtr, const TChar*>::value && !std::is_array<TPtr>::value>* = nullptr>
    constexpr basic_zstring_view(TPtr&& pStr) noexcept : BaseType(view_from_pointer(std::forward<TPtr>(pStr)))
    {
        validate_pointer();
    }

    constexpr basic_zstring_view(const std::basic_string<TChar>& str) noexcept : BaseType(&str[0], str.size())
    {
    }

    template <
        typename TSrc,
        std::enable_if_t<
            has_c_str<TSrc>::value && has_size<TSrc>::value && std::is_same_v<typename TSrc::value_type, TChar> &&
            !details::is_basic_zstring_view<std::decay_t<TSrc>>::value>* = nullptr>
    constexpr basic_zstring_view(TSrc const& src) noexcept : BaseType(view_from_pointer(src.c_str(), src.size()))
    {
        validate_pointer();
    }

    template <
        typename TSrc,
        std::enable_if_t<
            has_c_str<TSrc>::value && !has_size<TSrc>::value && std::is_same_v<typename TSrc::value_type, TChar> &&
            !details::is_basic_zstring_view<std::decay_t<TSrc>>::value>* = nullptr>
    constexpr basic_zstring_view(TSrc const& src) noexcept : BaseType(view_from_pointer(src.c_str()))
    {
        validate_pointer();
    }

    template <
        typename OtherTraits,
        std::enable_if_t<
            !std::is_same_v<Traits, OtherTraits> && std::is_same_v<BaseType, typename basic_zstring_view<TChar, OtherTraits>::BaseType> &&
            (!ZStringViewTraits::empty_strings_are_non_null || details::zstring_view_traits<TChar, OtherTraits>::empty_strings_are_non_null)>* = nullptr>
    constexpr basic_zstring_view(const basic_zstring_view<TChar, OtherTraits>& other) noexcept :
        BaseType(other.data(), other.size())
    {
    }

    template <
        typename OtherTraits,
        std::enable_if_t<
            !std::is_same_v<Traits, OtherTraits> && std::is_same_v<BaseType, typename basic_zstring_view<TChar, OtherTraits>::BaseType> &&
            ZStringViewTraits::empty_strings_are_non_null && !details::zstring_view_traits<TChar, OtherTraits>::empty_strings_are_non_null>* = nullptr>
    explicit constexpr basic_zstring_view(const basic_zstring_view<TChar, OtherTraits>& other) noexcept :
        BaseType(view_from_pointer(other.data(), other.size()))
    {
        validate_pointer();
    }

    template <typename OtherTraits, std::enable_if_t<!std::is_same_v<BaseType, typename basic_zstring_view<TChar, OtherTraits>::BaseType>>* = nullptr>
    basic_zstring_view(const basic_zstring_view<TChar, OtherTraits>&) = delete;

    template <
        typename OtherTraits,
        std::enable_if_t<!std::is_same_v<Traits, OtherTraits> && std::is_same_v<BaseType, typename basic_zstring_view<TChar, OtherTraits>::BaseType>>* = nullptr>
    constexpr basic_zstring_view& operator=(const basic_zstring_view<TChar, OtherTraits>& other) noexcept
    {
        const auto data = other.data();
        if constexpr (ZStringViewTraits::empty_strings_are_non_null && !details::zstring_view_traits<TChar, OtherTraits>::empty_strings_are_non_null)
        {
            if (data == nullptr)
            {
                WI_STL_FAIL_FAST_IF(data == nullptr);
                return *this;
            }
        }
        BaseType::operator=(BaseType(data, other.size()));
        return *this;
    }

    template <typename OtherTraits, std::enable_if_t<!std::is_same_v<BaseType, typename basic_zstring_view<TChar, OtherTraits>::BaseType>>* = nullptr>
    basic_zstring_view& operator=(const basic_zstring_view<TChar, OtherTraits>&) = delete;

    // basic_string_view [] precondition won't let us read view[view.size()]; so we define our own.
    WI_NODISCARD constexpr const TChar& operator[](size_type idx) const noexcept
    {
        WI_ASSERT(idx <= this->size() && this->data() != nullptr);
        return this->data()[idx];
    }

    WI_NODISCARD constexpr const TChar* c_str() const noexcept
    {
        WI_ASSERT(!ZStringViewTraits::empty_strings_are_non_null || (this->data() != nullptr));
        WI_ASSERT(this->data() == nullptr || this->data()[this->size()] == 0);
        return this->data();
    }

    // contains() backport for builds below C++23. Compiles out once the STL provides
    // basic_string_view::contains natively.
#if !defined(__cpp_lib_string_contains) || __cpp_lib_string_contains < 202011L
    WI_NODISCARD constexpr bool contains(BaseType view) const noexcept
    {
        return this->find(view) != this->npos;
    }

    WI_NODISCARD constexpr bool contains(TChar value) const noexcept
    {
        return this->find(value) != this->npos;
    }

    WI_NODISCARD constexpr bool contains(const TChar* value) const
    {
        return this->find(value) != this->npos;
    }
#endif

    WI_NODISCARD constexpr basic_zstring_view substr(size_type pos = 0) const
    {
        const auto tail = BaseType(*this).substr(pos);
        return tail.data() == nullptr ? basic_zstring_view{} : basic_zstring_view{tail.data(), tail.size()};
    }

    WI_NODISCARD constexpr BaseType substr(size_type pos, size_type count) const
    {
        return BaseType(*this).substr(pos, count);
    }

private:
    static constexpr BaseType default_view() noexcept
    {
        if constexpr (ZStringViewTraits::empty_strings_are_non_null)
        {
            return BaseType(&details::zstring_view_empty_storage<TChar>[0], 0);
        }
        else
        {
            return BaseType{};
        }
    }

    static constexpr BaseType view_from_pointer(const TChar* value) noexcept
    {
        if constexpr (ZStringViewTraits::empty_strings_are_non_null)
        {
            return BaseType(value, value == nullptr ? 0 : BaseType::traits_type::length(value));
        }
        else
        {
            return BaseType(value);
        }
    }

    static constexpr BaseType view_from_pointer(const TChar* value, size_type length) noexcept
    {
        if constexpr (ZStringViewTraits::empty_strings_are_non_null)
        {
            // Let constructor-body validation report null without first passing an invalid range to the base.
            return BaseType(value, value == nullptr ? 0 : length);
        }
        else
        {
            return BaseType(value, length);
        }
    }

    constexpr void validate_pointer() const noexcept
    {
        if constexpr (ZStringViewTraits::empty_strings_are_non_null)
        {
            WI_STL_FAIL_FAST_IF(this->data() == nullptr);
        }
    }

    constexpr void validate_pointer_and_terminator() const noexcept
    {
        const auto ptr = this->data();
        const auto len = this->size();
        if constexpr (ZStringViewTraits::empty_strings_are_non_null)
        {
            WI_STL_FAIL_FAST_IF((ptr == nullptr) || (ptr[len] != 0));
        }
        else
        {
            // Preserve nullable preconditions; the guard also keeps valid construction constexpr.
            if (ptr[len] != 0)
            {
                WI_STL_FAIL_FAST_IF(ptr[len] != 0);
            }
        }
    }

    // Bounds-checked version of char_traits::length, like strnlen. Requires that the input contains a null terminator.
    static constexpr size_type length_n(_In_reads_opt_(buf_size) const TChar* str, size_type buf_size) noexcept
    {
        const BaseType view(str, buf_size);
        auto pos = view.find_first_of(TChar());
        if (pos == view.npos)
        {
            WI_STL_FAIL_FAST_IF(true);
        }
        return pos;
    }

    // The following basic_string_view methods must not be allowed because they break the nul-termination.
    using BaseType::remove_suffix;
    using BaseType::swap;
};

using zstring_view = basic_zstring_view<char>;
using zwstring_view = basic_zstring_view<wchar_t>;

// Variants that reject null construction and default to a non-null empty string.
using nonnull_zstring_view = basic_zstring_view<char, nonnull_zstring_view_traits<char>>;
using nonnull_zwstring_view = basic_zstring_view<wchar_t, nonnull_zstring_view_traits<wchar_t>>;

// str_raw_ptr is an overloaded function that retrieves a const pointer to the first character in a string's buffer.
// This is the overload for std::wstring.  Other overloads available in resource.h.
template <typename TChar, typename Traits>
inline auto str_raw_ptr(basic_zstring_view<TChar, Traits> str)
{
    return str.c_str();
}

namespace details
{
    template <std::size_t N>
    struct wchar_literal_storage
    {
        static constexpr const std::size_t size = N;
        wchar_t value[N];
        constexpr wchar_literal_storage(const wchar_t (&str)[N]) WI_NOEXCEPT
        {
            std::copy_n(str, N, value);
        }
    };

    template <std::size_t N>
    wchar_literal_storage(const wchar_t (&)[N]) -> wchar_literal_storage<N>;
} // namespace details

inline namespace literals
{
#if __WI_LIBCPP_STD_VER >= 20
    template <wil::details::wchar_literal_storage Str>
    struct bstr_storage_t
    {
        uint32_t sizeBytes = static_cast<uint32_t>((Str.size - 1) * sizeof(wchar_t));
        decltype(Str) string{Str};
    };

    /**
        A statically-allocated, BSTR-shaped literal: a length-prefixed wide string whose data pointer is a valid
        BSTR (usable with SysStringLen, SysStringByteLen, wcslen). No heap allocation; size is the literal's exact
        length. Lifetime is tied to the literal object itself.

        Example:
            void Use(BSTR);
            Use(L"foo"_bstr);
    */
    template <wil::details::wchar_literal_storage Lit>
    WI_NODISCARD constexpr auto operator""_bstr() WI_NOEXCEPT
    {
        constexpr static const bstr_storage_t<Lit> storage{};
        static_assert(sizeof(storage.sizeBytes) == 4);
        static_assert(offsetof(decltype(storage), sizeBytes) == 0);
        static_assert(offsetof(decltype(storage), string) == sizeof(uint32_t));
        static_assert(offsetof(decltype(storage), string.value) == sizeof(uint32_t));
        return const_cast<wchar_t*>(storage.string.value);
    }

#endif // __WI_LIBCPP_STD_VER >= 20

    constexpr zstring_view operator""_zv(const char* str, std::size_t len) noexcept
    {
        return {str, len};
    }

    constexpr zwstring_view operator""_zv(const wchar_t* str, std::size_t len) noexcept
    {
        return {str, len};
    }
} // namespace literals

#endif // __cpp_lib_string_view >= 201606L

#if __WI_LIBCPP_STD_VER >= 17
// This is a helper that allows one to construct a functor that has an overloaded operator()
// composed from the operator()s of multiple lambdas. It is most useful as the "visitor" for a
// std::visit call on a std::variant. A lambda for each type in the variant, and optionally one
// generic lambda, can be provided to perform compile time visitation of the std::variant.
//
// Example:
//        std::variant<int, bool, double, void*> theVariant;
//        std::visit(wil::overloaded{
//           [](int theInt)
//           {
//                // Handle int.
//           },
//           [](double theDouble)
//           {
//                // Handle double.
//           },
//           [](auto boolOrVoidPtr)
//           {
//                // This will receive all the remaining types. Alternatively, handle each type with
//                // a lambda that accepts that type. If all types are not handled, you get a
//                // compile-time error, which makes std::visit superior to an if-else ladder that
//                // tries to handle each type in the variant.
//           }},
//           theVariant);
//
template <typename... T>
struct overloaded final : T...
{
    using T::operator()...;

    // This allows one to use the () syntax to construct the visitor, instead of {}. Both are
    // equivalent, and the choice ultimately boils down to preference of style.
    template <typename... Fs>
    constexpr explicit overloaded(Fs&&... fs) : T{std::forward<Fs>(fs)}...
    {
    }
};

// Deduction guide to aid CTAD.
template <typename... T>
overloaded(T...) -> overloaded<T...>;

#endif // __WI_LIBCPP_STD_VER >= 17

} // namespace wil

// This suppression is a temporary workaround to allow libraries built with C++20 to link into binaries built with
// earlier standard versions such as C++17. This appears to be an issue even when this specialization goes unused
#ifndef WIL_SUPPRESS_STD_FORMAT_USE
#if (__WI_LIBCPP_STD_VER >= 20) && WI_HAS_INCLUDE(<format>, 1) // Assume present if C++20
#include <format>
template <typename TChar, typename Traits>
struct std::formatter<wil::basic_zstring_view<TChar, Traits>, TChar>
    : std::formatter<std::basic_string_view<TChar, typename wil::details::zstring_view_traits<TChar, Traits>::char_traits>, TChar>
{
};
#endif
#endif

#endif // WIL_ENABLE_EXCEPTIONS

#endif // __WIL_STL_INCLUDED
