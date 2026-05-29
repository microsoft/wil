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

// WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER (opt-in, off by default):
//   When defined, basic_zstring_view{}'s data() returns a pointer to a static empty buffer
//   instead of nullptr. This makes c_str() always safe to dereference and aligns with the
//   design proposed for std::basic_zstring_view in P3655R0.
//
//   The default-off preserves source compatibility with code that uses data() == nullptr as
//   a "no string" sentinel and matches the documented default of std::basic_string_view.
//   New callers that want the safer default-construct behaviour opt in by defining the macro
//   ahead of including this header.
//
//   Linkage matrix (enforced by WI_ODR_PRAGMA via MSVC's /detect_mismatch). The relevant
//   axis is whether the macro is defined when each translation unit includes this header:
//     * Defined in both:    instances interop normally; default ctor returns a non-null empty buffer.
//     * Undefined in both:  instances interop normally; default ctor returns nullptr (base behaviour).
//     * Mismatched:         link error LNK2038 on the mismatch tag
//                           'ODR_violation_WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER_mismatch'.
#if defined(WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER)
WI_ODR_PRAGMA("WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER", "1")
#else
WI_ODR_PRAGMA("WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER", "0")
#endif

/**
    `basic_zstring_view<TChar>` is a non-owning, read-only view of a *null-terminated* sequence of `TChar`.

    Core invariant: a constructed `basic_zstring_view` always satisfies `data()[size()] == TChar()`.
    The class adds null-termination guarantees to `std::basic_string_view`, making it suitable for passing
    to C APIs that require dereferenceable null-terminated strings (e.g. `printf("%s", v.c_str())`,
    `fopen(v.c_str(), ...)`). Where `std::basic_string_view` requires the caller to remember the
    null-termination contract, `basic_zstring_view` enforces it at construction.

    - A `basic_zstring_view` decays implicitly to `std::basic_string_view` (it inherits from it).
    - A `basic_zstring_view` can be constructed from a string literal, a `(const TChar*, size_type)` pair
      (with a debug fail-fast verifying the null terminator), a `std::basic_string`, or any user-defined
      type that exposes `c_str()` (and optionally `size()`) returning `TChar`.

    Default construction matches `std::basic_string_view`: `data() == nullptr`, `size() == 0`. Callers
    that want a default-constructed view whose `c_str()` is always safe to dereference can opt in by
    defining `WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER` before including this header; that flips the default
    constructor to point `data()` at an internal static empty buffer, aligning with the design proposed
    for `std::basic_zstring_view` in P3655R0 (https://wg21.link/p3655r0).

    The substr family follows the design proposed for `std::zstring_view` in P3655R0
    (https://wg21.link/p3655r0): the one-argument `substr(pos)` returns a `basic_zstring_view`
    (the tail of a null-terminated string is itself null-terminated), and the two-argument
    `substr(pos, count)` returns a `std::basic_string_view` because an arbitrary slice is
    generally not null-terminated.

    @note Public inheritance from `std::basic_string_view` means a caller can bypass the
    null-termination invariant by casting to the base, e.g.
    `static_cast<std::basic_string_view<char>&>(zv) = sv` where `sv` is a non-null-terminated view.
    P3655R0's proposed `std::zstring_view` avoids this by not inheriting. The invariant holds for
    normal use but isn't airtight against this kind of base-class cast.
*/
template <class TChar>
class basic_zstring_view : public std::basic_string_view<TChar>
{
    using size_type = typename std::basic_string_view<TChar>::size_type;

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
#if defined(WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER)
    /**
     * Default-construct a view of an internal static empty buffer.
     * Yields `data() != nullptr`, `size() == 0`, and `c_str()[0] == TChar()` so the result is safe
     * to hand to any C API expecting a dereferenceable null-terminated string.
     */
    constexpr basic_zstring_view() noexcept : std::basic_string_view<TChar>(&_empty_storage[0], 0)
    {
    }
#else
    /**
     * Default-construct a view matching `std::basic_string_view`: `data() == nullptr`, `size() == 0`.
     * Define `WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER` before including this header to flip the default to
     * a non-null empty buffer (see the class documentation).
     */
    constexpr basic_zstring_view() noexcept = default;
#endif
    constexpr basic_zstring_view(const basic_zstring_view&) noexcept = default;
    constexpr basic_zstring_view& operator=(const basic_zstring_view&) noexcept = default;

    constexpr basic_zstring_view(_In_reads_z_(stringLength + 1) const TChar* pStringData, size_type stringLength) noexcept :
        std::basic_string_view<TChar>(pStringData, stringLength)
    {
        if (pStringData[stringLength] != 0)
        {
            WI_STL_FAIL_FAST_IF(true);
        }
    }

    template <size_t stringArrayLength>
    constexpr basic_zstring_view(const TChar (&stringArray)[stringArrayLength]) noexcept :
        std::basic_string_view<TChar>(&stringArray[0], length_n(&stringArray[0], stringArrayLength))
    {
    }

    // Construct from nul-terminated char ptr. To prevent this from overshadowing array construction,
    // we disable this constructor if the value is an array (including string literal).
    template <typename TPtr, std::enable_if_t<std::is_convertible<TPtr, const TChar*>::value && !std::is_array<TPtr>::value>* = nullptr>
    constexpr basic_zstring_view(_In_z_ TPtr&& pStr) noexcept : std::basic_string_view<TChar>(std::forward<TPtr>(pStr))
    {
    }

    constexpr basic_zstring_view(const std::basic_string<TChar>& str) noexcept :
        std::basic_string_view<TChar>(&str[0], str.size())
    {
    }

    template <typename TSrc, std::enable_if_t<has_c_str<TSrc>::value && has_size<TSrc>::value && std::is_same_v<typename TSrc::value_type, TChar>>* = nullptr>
    constexpr basic_zstring_view(TSrc const& src) noexcept : std::basic_string_view<TChar>(src.c_str(), src.size())
    {
    }

    template <typename TSrc, std::enable_if_t<has_c_str<TSrc>::value && !has_size<TSrc>::value && std::is_same_v<typename TSrc::value_type, TChar>>* = nullptr>
    constexpr basic_zstring_view(TSrc const& src) noexcept : std::basic_string_view<TChar>(src.c_str())
    {
    }

    // basic_string_view [] precondition won't let us read view[view.size()]; so we define our own.
    WI_NODISCARD constexpr const TChar& operator[](size_type idx) const noexcept
    {
        WI_ASSERT(idx <= this->size() && this->data() != nullptr);
        return this->data()[idx];
    }

    WI_NODISCARD _Ret_z_ constexpr const TChar* c_str() const noexcept
    {
        WI_ASSERT(this->data() == nullptr || this->data()[this->size()] == 0);
        return this->data();
    }

    // contains() backport for builds below C++23. Compiles out once the STL provides
    // basic_string_view::contains natively (via __cpp_lib_string_contains).
#if !defined(__cpp_lib_string_contains) || __cpp_lib_string_contains < 202011L
    WI_NODISCARD constexpr bool contains(std::basic_string_view<TChar> sv) const noexcept
    {
        return std::basic_string_view<TChar>(*this).find(sv) != std::basic_string_view<TChar>::npos;
    }

    WI_NODISCARD constexpr bool contains(TChar ch) const noexcept
    {
        return std::basic_string_view<TChar>(*this).find(ch) != std::basic_string_view<TChar>::npos;
    }

    WI_NODISCARD constexpr bool contains(_In_z_ const TChar* s) const
    {
        return std::basic_string_view<TChar>(*this).find(s) != std::basic_string_view<TChar>::npos;
    }
#endif // !defined(__cpp_lib_string_contains) || __cpp_lib_string_contains < 202011L

    /**
     * Returns a `basic_zstring_view` of the tail of this view, starting at `pos`.
     *
     * The result is null-terminated because the tail of a null-terminated string is itself
     * null-terminated.
     *
     * @param pos starting position (default 0). Must satisfy `pos <= size()`.
     * @throws std::out_of_range if `pos > size()` (propagated from `std::basic_string_view::substr`).
     */
    WI_NODISCARD constexpr basic_zstring_view substr(size_type pos = 0) const
    {
        const auto tail = std::basic_string_view<TChar>(*this).substr(pos);
        // Short-circuit the (nullptr, 0) tail case (substr(0) on a default-constructed view
        // when WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER is not defined) so we don't round-trip a null
        // pointer through the verifying (ptr, length) constructor.
        if (tail.data() == nullptr)
        {
            return basic_zstring_view{};
        }
        return basic_zstring_view(tail.data(), tail.size());
    }

    // Re-declared (not just inherited) so the one-arg substr(pos) above doesn't hide the
    // inherited two-arg overload via standard name-hiding rules.
    /**
     * Returns a `std::basic_string_view` of an arbitrary sub-range.
     *
     * The return type is `std::basic_string_view` rather than `basic_zstring_view` because the
     * sub-range is generally not null-terminated. Callers who need a null-terminated tail should
     * use the one-argument `substr(pos)` overload above.
     *
     * @param pos starting position. Must satisfy `pos <= size()`.
     * @param count maximum number of characters in the resulting sub-range. Clamped to `size() - pos`.
     * @throws std::out_of_range if `pos > size()` (propagated from `std::basic_string_view::substr`).
     */
    WI_NODISCARD constexpr std::basic_string_view<TChar> substr(size_type pos, size_type count) const
    {
        return std::basic_string_view<TChar>(*this).substr(pos, count);
    }

private:
    // Bounds-checked version of char_traits::length, like strnlen. Requires that the input contains a null terminator.
    static constexpr size_type length_n(_In_reads_opt_(buf_size) const TChar* str, size_type buf_size) noexcept
    {
        const std::basic_string_view<TChar> view(str, buf_size);
        auto pos = view.find_first_of(TChar());
        if (pos == view.npos)
        {
            WI_STL_FAIL_FAST_IF(true);
        }
        return pos;
    }

    // The following basic_string_view methods must not be allowed because they break the nul-termination.
    using std::basic_string_view<TChar>::swap;
    using std::basic_string_view<TChar>::remove_suffix;

    // Backing buffer for the default-constructed view. Only present when the safer default is opted in.
#if defined(WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER)
    static inline constexpr TChar _empty_storage[1]{TChar()};
#endif
};

using zstring_view = basic_zstring_view<char>;
using zwstring_view = basic_zstring_view<wchar_t>;

// str_raw_ptr is an overloaded function that retrieves a const pointer to the first character in a string's buffer.
// This is the overload for std::wstring.  Other overloads available in resource.h.
template <typename TChar>
inline auto str_raw_ptr(basic_zstring_view<TChar> str)
{
    return str.c_str();
}

inline namespace literals
{
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
template <typename TChar>
struct std::formatter<wil::basic_zstring_view<TChar>, TChar> : std::formatter<std::basic_string_view<TChar>, TChar>
{
};
#endif
#endif

#endif // WIL_ENABLE_EXCEPTIONS

#endif // __WIL_STL_INCLUDED
