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

/// @cond
namespace details
{
    // SFINAE detector for the `empty_strings_are_non_null` opt-in marker on a Traits type.
    // Mirrors MSVC STL's `_Get_propagate_on_container_copy` allocator_traits detection idiom.
    template <class T, class = void>
    struct zsv_empty_is_nonnull : std::false_type
    {
    };

    template <class T>
    struct zsv_empty_is_nonnull<T, std::void_t<typename T::empty_strings_are_non_null>> : T::empty_strings_are_non_null
    {
    };

    template <class T>
    inline constexpr bool zsv_empty_is_nonnull_v = zsv_empty_is_nonnull<T>::value;

    // Sentinel buffer for the safe variant's default-constructed view. Variable template so
    // it's lazily instantiated and only emitted for TChars actually default-constructed
    // through a safe `basic_zstring_view`; the default variant never references it.
    // Parameterized on TChar (not Traits) so all safe instantiations sharing a char type
    // COMDAT-fold onto a single byte of `.rdata`.
    template <typename TChar>
    inline constexpr TChar zsv_empty_storage[1]{TChar()};
} // namespace details
/// @endcond

/**
    `zstring_view_traits_safe<TChar>` is a `std::char_traits<TChar>`-compatible traits type that opts
    `basic_zstring_view` into the "non-null empty buffer" behaviour proposed for
    `std::basic_zstring_view` in P3655R0. Instantiating
    `basic_zstring_view<TChar, zstring_view_traits_safe<TChar>>` (aliased as `wil::zstring_view_safe`
    / `wil::zwstring_view_safe`) yields a view whose `data() != nullptr` is invariant: a
    default-constructed view points at an internal static empty buffer and `c_str()[0] == TChar()`
    is always safe to dereference.

    The nested type `empty_strings_are_non_null = std::true_type;` is detected by
    `wil::details::zsv_empty_is_nonnull_v`. Default char_traits do not carry the marker, so
    `basic_zstring_view<TChar>` (with the default traits) retains the original WIL semantics where
    a default-constructed view has `data() == nullptr`.
*/
template <class TChar>
struct zstring_view_traits_safe : std::char_traits<TChar>
{
    using empty_strings_are_non_null = std::true_type;
};

/**
    `basic_zstring_view<TChar, Traits>` is a non-owning, read-only view of a *null-terminated*
    sequence of `TChar`. The class adds null-termination guarantees to `std::basic_string_view`,
    making it suitable for passing to C APIs that require dereferenceable null-terminated strings
    (e.g. `printf("%s", v.c_str())`, `fopen(v.c_str(), ...)`).

    The `Traits` template parameter selects between two behavioural variants. Default
    (`Traits = std::char_traits<TChar>`) preserves the original WIL semantics: a default-constructed
    view has `data() == nullptr` and `c_str()` returns null. The opt-in safe variant
    (`Traits = zstring_view_traits_safe<TChar>`) makes `data() != nullptr` invariant: default
    construction points at an internal static empty buffer and `c_str()` is always safe to
    dereference. Type aliases `wil::zstring_view` / `wil::zwstring_view` select the default
    variant; `wil::zstring_view_safe` / `wil::zwstring_view_safe` select the safe variant. Both
    variants can be used in the same translation unit; the choice is per-call-site, not per-binary.

    Both variants:
      - Construct from a string literal, a `(const TChar*, size_type)` pair (with a fail-fast
        verifying the null terminator), a `std::basic_string`, or any user-defined type that
        exposes `c_str()` (and optionally `size()`) returning `TChar`. The safe variant
        additionally fail-fasts on a null `pStringData`; the default variant treats null as
        a precondition violation (latent UB on the null-terminator deref), matching the prior
        WIL class behavior. The safe variant also `= delete`s the `(std::nullptr_t)` overload
        (tracks P3655R0 and P2166R1's direction); the default variant leaves the overload
        accessible so existing C++17/C++20 call sites that construct from a `nullptr` literal
        continue to compile (such code was always latent UB at first use, but breaking it now
        would be a source-compat regression for existing WIL consumers).
      - Provide an explicit converting constructor from `std::basic_string_view<TChar>` that
        fail-fasts on a null pointer (the only way `std::basic_string_view` can violate the
        non-null contract) and then delegates to the validating `(ptr, len)` ctor.
      - `substr(pos)` returns a `basic_zstring_view` (the tail of a null-terminated string is
        itself null-terminated); follows the design proposed for `std::zstring_view` in P3655R0.
      - `substr(pos, count)` returns a `std::basic_string_view<TChar, Traits>` because an
        arbitrary slice is generally not null-terminated.

    @note **Slicing risk.** Public inheritance from `std::basic_string_view<TChar, Traits>` means
    a caller can bypass the null-termination invariant by binding to the base, e.g.
    `static_cast<std::basic_string_view<char>&>(zv) = sv` where `sv` is a non-null-terminated
    view. This also re-exposes hidden invariant-breakers (`remove_suffix`, `swap`) on the slice.
    P3655R0's proposed `std::zstring_view` avoids this by not inheriting; WIL retains inheritance
    for source compatibility with prior versions of this class and for broad-ecosystem
    `std::basic_string_view` interop. The invariant holds for normal use but is not airtight
    against base-class slicing.

    @note **Hidden (not deleted) invariant-breakers.** `remove_suffix(n)` and `swap(other)` are
    inaccessible from `basic_zstring_view` directly (compile error: "inaccessible") via private
    using-declarations. They remain reachable via base-class slice/cast (see slicing caveat
    above). The 2-arg `substr(pos, count)` is permitted but returns the base
    `std::basic_string_view<TChar, Traits>` because an arbitrary slice may not be null-terminated.

    @note **Cross-instantiation conversion.**
      - `zstring_view_safe` converts **implicitly** to `zstring_view` - the more-constrained type
        collapses into the less-constrained one without runtime cost. The collapse is sound
        because `zstring_view_traits_safe<TChar>` derives from `std::char_traits<TChar>` with no
        behavioural overrides, so the two `Traits` have identical `compare`/`eq`/`length`
        semantics. The shape of the asymmetry (more-constrained implicit, less-constrained
        explicit) parallels `std::span<T, N>` -> `std::span<T, dynamic_extent>` (C++20).
      - The reverse direction (default -> safe) is an **explicit** converting constructor on the
        safe variant: `zstring_view_safe{view}`. It delegates to the safe variant's validating
        `(pointer, length)` constructor, which fail-fasts on a null pointer (so calling it on a
        default-constructed `zstring_view` - whose `data()` is null - fail-fasts on conversion).
        The `explicit` keyword keeps the safety transition visible at the call site.
      - The safe variant also converts **implicitly** to `std::basic_string_view<TChar>` (default
        traits), enabling drop-in use with any API taking `std::string_view`. This is a
        pointer+size rewrap; the safe variant's invariants are already enforced before conversion,
        so the resulting `std::string_view` is always non-null (though the receiver does not know
        that statically). The conversion is gated to the canonical
        `zstring_view_traits_safe<TChar>` (not any `Traits` that merely derives from it): a user
        who extends the traits with custom `eq`/`lt`/`compare` (e.g. case-insensitive) is not
        silently demoted to default `char_traits` semantics on conversion.
      - Cross-variant comparison works out of the box: a `zstring_view_safe` can be compared to a
        `zstring_view` (and vice versa). Both directions bind to `std::basic_string_view<TChar>`'s
        `operator==` via the existing implicit conversions (safe -> base via
        `operator basic_string_view<TChar>()`, default -> base via inherited-base slicing). No
        special cross-Traits overloads needed.

    @note **Overload-resolution caveat (safe variant only).** Because
    `zstring_view_safe -> zstring_view` and `zstring_view_safe -> std::basic_string_view<TChar>`
    are both single user-defined conversions, a call site that has both `f(wil::zstring_view)` and
    `f(std::string_view)` overloads will be ambiguous when passed a `zstring_view_safe`.
    Disambiguate at the call site with `f(wil::zstring_view{safe_view})` or
    `f(std::string_view{safe_view})`. The ambiguity does not arise for the default variant
    (identity match wins) or when only one of the two overloads is present.

    @note **Safe variant `(nullptr, 0)` policy.** The safe variant's `(ptr, len)` ctor fail-fasts
    on a null pointer regardless of length; `(nullptr, 0)` is not a "harmless empty" shortcut.
    P3655R0's invariant - `[data(), data() + size()]` is a valid range with `data() + size()`
    pointing at `charT()` - cannot be satisfied when `data()` is null, even at size zero. Same
    `WI_STL_FAIL_FAST_IF` primitive (and same `REQUIRE_ERROR`-style testability) as the existing
    no-NULL-in-range fail-fast that both variants have shipped for years. Callers wanting "empty
    intent" should default-construct (`zstring_view_safe{}`), which yields a dereferenceable empty
    buffer at zero runtime cost.
*/
template <class TChar, class Traits = std::char_traits<TChar>>
class basic_zstring_view : public std::basic_string_view<TChar, Traits>
{
    using size_type = typename std::basic_string_view<TChar, Traits>::size_type;

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

    // Tag for the trusted (non-validating) private (ptr, len) ctor used by substr() and other
    // internal sites that have already established the invariants.
    struct _trusted_tag
    {
    };

public:
    /**
     * Default-construct a view.
     * - Default Traits (`zstring_view` / `zwstring_view`): `data() == nullptr`, `size() == 0` -
     *   matches `std::basic_string_view`.
     * - Safe Traits (`zstring_view_safe` / `zwstring_view_safe`): `data() != nullptr`,
     *   `size() == 0`, `c_str()[0] == TChar()` - the view points at an internal static empty
     *   buffer and is safe to hand to any C API expecting a dereferenceable null-terminated string.
     */
    constexpr basic_zstring_view() noexcept
    {
        if constexpr (details::zsv_empty_is_nonnull_v<Traits>)
        {
            // Default-init of the base produced (nullptr, 0); re-seat with the sentinel.
            static_cast<std::basic_string_view<TChar, Traits>&>(*this) =
                std::basic_string_view<TChar, Traits>(&details::zsv_empty_storage<TChar>[0], 0);
        }
    }

    constexpr basic_zstring_view(const basic_zstring_view&) noexcept = default;
    constexpr basic_zstring_view& operator=(const basic_zstring_view&) noexcept = default;

    // Deleted on the safe variant - tracks the standard direction (P2166R1 deletes the
    // analogous overload on `basic_string_view` in C++23; P3655R0 carries the same rule
    // through for the proposed `std::zstring_view`). Left intact on the default variant so
    // existing C++17/C++20 callers that pass a `nullptr` literal continue to compile (the
    // resulting view is latent UB at first use, but breaking that source pattern now would
    // be a compat regression for existing WIL consumers).
    template <typename T = Traits, std::enable_if_t<details::zsv_empty_is_nonnull_v<T>, int> = 0>
    basic_zstring_view(std::nullptr_t) = delete;

    // (ptr, len) ctor. Both variants take `_In_reads_z_` (non-null is part of the contract); the
    // variants differ only in runtime enforcement of that contract, and in how the two diverge
    // under test-harness fail-fast detours (e.g. witest's `DoesCodeFailFast`, which intercepts
    // `ReportFailure_NoReturn` and returns rather than aborting):
    // - Default Traits: trusts the caller. Null is latent UB on the null-terminator deref below.
    //   Matches the prior WIL class behavior. The test suite deliberately never passes
    //   `(nullptr, N)` to this variant - there is no fail-fast to detour, so a test would just
    //   real-AV.
    // - Safe Traits: fail-fasts on a null pointer via `_require_non_null` in the init list, so
    //   the diagnosis runs before the base ctor sees a malformed `(ptr, len)` pair. The body
    //   additionally guards the trailing-null read on `pStringData != nullptr` so a detoured
    //   fail-fast returns instead of real-AVing after `_require_non_null` hands back the (null)
    //   pointer.
    template <typename T = Traits, std::enable_if_t<!details::zsv_empty_is_nonnull_v<T>, int> = 0>
    constexpr basic_zstring_view(_In_reads_z_(stringLength + 1) const TChar* pStringData, size_type stringLength) noexcept :
        std::basic_string_view<TChar, Traits>(pStringData, stringLength)
    {
        if (pStringData[stringLength] != 0)
        {
            WI_STL_FAIL_FAST_IF(true);
        }
    }

    template <typename T = Traits, std::enable_if_t<details::zsv_empty_is_nonnull_v<T>, int> = 0>
    constexpr basic_zstring_view(_In_reads_z_(stringLength + 1) const TChar* pStringData, size_type stringLength) noexcept :
        std::basic_string_view<TChar, Traits>(_require_non_null(pStringData), stringLength)
    {
        if (pStringData != nullptr && pStringData[stringLength] != 0)
        {
            WI_STL_FAIL_FAST_IF(true);
        }
    }

    template <size_t stringArrayLength>
    constexpr basic_zstring_view(const TChar (&stringArray)[stringArrayLength]) noexcept :
        std::basic_string_view<TChar, Traits>(&stringArray[0], length_n(&stringArray[0], stringArrayLength))
    {
    }

    // Construct from nul-terminated char ptr. To prevent this from overshadowing array construction,
    // we disable this constructor if the value is an array (including string literal).
    // Note: no `_In_z_` annotation because TPtr can be a class type with a conversion operator,
    // so the SAL annotation does not apply cleanly.
    template <typename TPtr, std::enable_if_t<std::is_convertible<TPtr, const TChar*>::value && !std::is_array<TPtr>::value>* = nullptr>
    constexpr basic_zstring_view(TPtr&& pStr) noexcept : std::basic_string_view<TChar, Traits>(std::forward<TPtr>(pStr))
    {
    }

    constexpr basic_zstring_view(const std::basic_string<TChar>& str) noexcept :
        std::basic_string_view<TChar, Traits>(&str[0], str.size())
    {
    }

    template <typename TSrc, std::enable_if_t<has_c_str<TSrc>::value && has_size<TSrc>::value && std::is_same_v<typename TSrc::value_type, TChar>>* = nullptr>
    constexpr basic_zstring_view(TSrc const& src) noexcept : std::basic_string_view<TChar, Traits>(src.c_str(), src.size())
    {
    }

    template <typename TSrc, std::enable_if_t<has_c_str<TSrc>::value && !has_size<TSrc>::value && std::is_same_v<typename TSrc::value_type, TChar>>* = nullptr>
    constexpr basic_zstring_view(TSrc const& src) noexcept : std::basic_string_view<TChar, Traits>(src.c_str())
    {
    }

    /**
     * Explicit converting constructor from `std::basic_string_view<TChar>`. Fail-fasts if the
     * source view has `data() == nullptr` (the only way `std::basic_string_view` can violate the
     * non-null contract); otherwise delegates to the validating `(ptr, len)` ctor.
     */
    constexpr explicit basic_zstring_view(std::basic_string_view<TChar> sv) noexcept :
        basic_zstring_view(_require_non_null(sv.data()), sv.size())
    {
    }

    // Cross-variant converting ctors. SFINAE-gated so each direction exists only on the relevant
    // variant:
    // - On the default variant: implicit ctor accepting the safe variant. The collapse is sound
    //   because `zstring_view_traits_safe<TChar>` derives from `std::char_traits<TChar>` with no
    //   behavioural overrides, so the two Traits have identical compare/eq/length semantics. The
    //   shape of the asymmetry (more-constrained implicit, less-constrained explicit) parallels
    //   `std::span<T, N>` -> `std::span<T, dynamic_extent>` (C++20).
    // - On the safe variant: explicit ctor accepting the default variant (delegates to the
    //   validating (ptr, len) ctor, which fail-fasts on null).
    // NOTE: these cross-variant ctors take their argument by const-reference rather than by value.
    // The standard treats a ctor whose first parameter is the enclosing class type as a copy ctor
    // and forbids the by-value form (it would recurse infinitely). For the default-Traits
    // instantiation `basic_zstring_view<TChar, std::char_traits<TChar>>` the line below would be
    // exactly that by-value copy ctor signature if we took the argument by value - the diagnostic
    // ("illegal copy constructor: first parameter must not be ...") fires before SFINAE has a
    // chance to remove the overload, so by-const-ref is the only signature that compiles.
    template <typename T = Traits, std::enable_if_t<!details::zsv_empty_is_nonnull_v<T>, int> = 0>
    constexpr basic_zstring_view(basic_zstring_view<TChar, zstring_view_traits_safe<TChar>> const& safe) noexcept :
        std::basic_string_view<TChar, Traits>(safe.data(), safe.size())
    {
    }

    template <typename T = Traits, std::enable_if_t<details::zsv_empty_is_nonnull_v<T>, int> = 0>
    constexpr explicit basic_zstring_view(basic_zstring_view<TChar, std::char_traits<TChar>> const& other) noexcept :
        basic_zstring_view(other.data(), other.size())
    {
    }

    // Implicit conversion to default-traits `std::basic_string_view`.
    //
    // Only present on the safe variant. For the shipped default aliases (`wil::zstring_view` /
    // `wil::zwstring_view`, where `Traits = std::char_traits<TChar>`), the inherited base IS
    // already `std::basic_string_view<TChar>` so slicing provides this conversion for free;
    // adding the operator there would be redundant. (A user-instantiated default variant with
    // custom Traits has a different inherited base and would slice to that base instead.)
    //
    // Only present when `Traits` is exactly `zstring_view_traits_safe<TChar>` (not any Traits
    // that merely carries the safe marker). A user who extends `zstring_view_traits_safe<TChar>`
    // with custom eq/lt/compare (e.g. case-insensitive) would otherwise silently lose those
    // semantics when converted to default-traits `std::basic_string_view<TChar>`. The
    // std library deliberately omits implicit `basic_string_view<C, TraitsA>` ->
    // `basic_string_view<C, TraitsB>` conversions for the same reason. Extenders who genuinely
    // want the lossy conversion can write `std::basic_string_view<TChar>(view.data(), view.size())`
    // (one line, intent visible).
    //
    // The return type is hidden behind `enable_if_t` to make it template-dependent on `T`. This
    // sidesteps clang's `-Wclass-conversion` warning, which fires syntactically on the default-Traits
    // instantiation (where `std::basic_string_view<TChar>` happens to be the inherited base) before
    // SFINAE removes the operator from the candidate set. A template-dependent return type forces
    // clang to defer the base-class shape check until template-argument deduction, at which point
    // SFINAE has already eliminated the candidate for default-Traits.
    template <typename T = Traits>
    constexpr operator std::enable_if_t<std::is_same_v<T, Traits> && std::is_same_v<Traits, zstring_view_traits_safe<TChar>>, std::basic_string_view<TChar>>() const noexcept
    {
        return {this->data(), this->size()};
    }

    // basic_string_view [] precondition won't let us read view[view.size()]; so we define our own.
    WI_NODISCARD constexpr const TChar& operator[](size_type idx) const noexcept
    {
        WI_ASSERT(idx <= this->size() && this->data() != nullptr);
        return this->data()[idx];
    }

    // `_Ret_maybenull_z_` rather than per-Traits split: SAL is a preprocessor-time annotation
    // on the declaration and can't vary with template parameters resolved at instantiation,
    // and per-Traits SFINAE overloads would change c_str()'s mangled name (source-compat hazard
    // for any caller taking `&basic_zstring_view::c_str`). `_Ret_maybenull_z_` is truthful for
    // both variants (loose but correct for the safe variant, whose invariant guarantees non-null).
    // Callers of the safe variant who want analyzer help can assert non-null at the call site.
    WI_NODISCARD _Ret_maybenull_z_ constexpr const TChar* c_str() const noexcept
    {
        WI_ASSERT(this->data() == nullptr || this->data()[this->size()] == 0);
        return this->data();
    }

    // contains() backport for builds below C++23. Compiles out once the STL provides
    // basic_string_view::contains natively (via __cpp_lib_string_contains).
#if !defined(__cpp_lib_string_contains) || __cpp_lib_string_contains < 202011L
    WI_NODISCARD constexpr bool contains(std::basic_string_view<TChar, Traits> sv) const noexcept
    {
        return std::basic_string_view<TChar, Traits>(*this).find(sv) != std::basic_string_view<TChar, Traits>::npos;
    }

    WI_NODISCARD constexpr bool contains(TChar ch) const noexcept
    {
        return std::basic_string_view<TChar, Traits>(*this).find(ch) != std::basic_string_view<TChar, Traits>::npos;
    }

    WI_NODISCARD constexpr bool contains(_In_z_ const TChar* s) const
    {
        return std::basic_string_view<TChar, Traits>(*this).find(s) != std::basic_string_view<TChar, Traits>::npos;
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
        const auto tail = std::basic_string_view<TChar, Traits>(*this).substr(pos);
        if constexpr (!details::zsv_empty_is_nonnull_v<Traits>)
        {
            // Short-circuit the (nullptr, 0) tail case (substr(0) on a default-constructed
            // default-traits view) so we don't round-trip a null pointer through the trusted
            // ctor's base initializer.
            if (tail.data() == nullptr)
            {
                return basic_zstring_view{};
            }
        }
        // Already validated by construction; use the trusted ctor to skip re-validation.
        return basic_zstring_view(_trusted_tag{}, tail.data(), tail.size());
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
    WI_NODISCARD constexpr std::basic_string_view<TChar, Traits> substr(size_type pos, size_type count) const
    {
        return std::basic_string_view<TChar, Traits>(*this).substr(pos, count);
    }

private:
    // Trusted (non-validating) ctor used by substr() and other internal sites that have already
    // established the invariants.
    constexpr basic_zstring_view(_trusted_tag, const TChar* p, size_type n) noexcept : std::basic_string_view<TChar, Traits>(p, n)
    {
    }

    // Self-guard helper for the explicit `std::basic_string_view<TChar>` ctor and for the safe
    // variant's `(ptr, len)` ctor.
    static constexpr const TChar* _require_non_null(const TChar* p) noexcept
    {
        if (p == nullptr)
        {
            WI_STL_FAIL_FAST_IF(true);
        }
        return p;
    }

    // Bounds-checked version of char_traits::length, like strnlen. Requires that the input contains a null terminator.
    static constexpr size_type length_n(_In_reads_opt_(buf_size) const TChar* str, size_type buf_size) noexcept
    {
        const std::basic_string_view<TChar, Traits> view(str, buf_size);
        auto pos = view.find_first_of(TChar());
        if (pos == view.npos)
        {
            WI_STL_FAIL_FAST_IF(true);
        }
        return pos;
    }

    // The following basic_string_view methods must not be allowed because they break the nul-termination.
    using std::basic_string_view<TChar, Traits>::swap;
    using std::basic_string_view<TChar, Traits>::remove_suffix;
};

using zstring_view = basic_zstring_view<char>;
using zwstring_view = basic_zstring_view<wchar_t>;
using zstring_view_safe = basic_zstring_view<char, zstring_view_traits_safe<char>>;
using zwstring_view_safe = basic_zstring_view<wchar_t, zstring_view_traits_safe<wchar_t>>;

// str_raw_ptr is an overloaded function that retrieves a const pointer to the first character in a string's buffer.
// This is the overload for basic_zstring_view (both variants).  Other overloads available in resource.h.
template <typename TChar, typename Traits>
inline auto str_raw_ptr(basic_zstring_view<TChar, Traits> str)
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

    constexpr zstring_view_safe operator""_zvs(const char* str, std::size_t len) noexcept
    {
        return zstring_view_safe{str, len};
    }

    constexpr zwstring_view_safe operator""_zvs(const wchar_t* str, std::size_t len) noexcept
    {
        return zwstring_view_safe{str, len};
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
struct std::formatter<wil::basic_zstring_view<TChar, Traits>, TChar> : std::formatter<std::basic_string_view<TChar, Traits>, TChar>
{
};
#endif
#endif

#endif // WIL_ENABLE_EXCEPTIONS

#endif // __WIL_STL_INCLUDED
