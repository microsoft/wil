#include "pch.h"

#include "common.h"

// Disable tests if we're not using exceptions. This is simpler than conditionally compiling this file
#ifdef WIL_ENABLE_EXCEPTIONS

#include <wil/stl.h>

struct dummy
{
    char value;
};

using namespace wil::literals;

// Specialize std::allocator<> so that we don't actually allocate/deallocate memory
static dummy g_memoryBuffer[256];
namespace std
{
template <>
struct allocator<dummy>
{
    using value_type = dummy;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    dummy* allocate(std::size_t count)
    {
        REQUIRE(count <= std::size(g_memoryBuffer));
        return g_memoryBuffer;
    }

    void deallocate(dummy* ptr, std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            REQUIRE(ptr[i].value == 0);
        }
    }
};
} // namespace std

TEST_CASE("StlTests::TestSecureAllocator", "[stl][secure_allocator]")
{
    {
        wil::secure_vector<dummy> sensitiveBytes(32, dummy{'a'});
    }
}

struct CustomNoncopyableString
{
    CustomNoncopyableString() = default;
    CustomNoncopyableString(const CustomNoncopyableString&) = delete;
    void operator=(const CustomNoncopyableString&) = delete;

    constexpr operator PCSTR() const
    {
        return "hello";
    }
    constexpr operator PCWSTR() const
    {
        return L"w-hello";
    }
};

TEST_CASE("StlTests::TestZStringView", "[stl][zstring_view]")
{
    // A default-constructed nullable view is empty and has data() == nullptr (matches the prior
    // single-class behaviour). The safe variant gets its own coverage in TestZStringViewSafe
    // and in the templated matrix below.
    REQUIRE(wil::zstring_view{}.empty());

    // Test empty string cases
    REQUIRE(wil::zstring_view{""}[0] == '\0');
    REQUIRE(wil::zstring_view{""}.c_str()[0] == '\0');
    REQUIRE(wil::zstring_view{""}.empty());

    // Test different constructor equality
    constexpr wil::zstring_view fromLiteral = "abc";
    REQUIRE(fromLiteral.length() == strlen("abc"));

    std::string stlString = "abc";
    wil::zstring_view fromString(stlString);
    wil::zstring_view fromPtr(stlString.data());

    static constexpr char charArray[] = "abc";
    constexpr wil::zstring_view fromArray(charArray);

    static constexpr char extendedCharArray[] = "abc\0\0\0\0\0";
    constexpr wil::zstring_view fromExtendedArray(extendedCharArray);

    wil::zstring_view copy = fromLiteral;

    REQUIRE(fromLiteral == stlString);
    REQUIRE(fromLiteral == fromString);
    REQUIRE(fromLiteral == fromArray);
    REQUIRE(fromLiteral == fromExtendedArray);
    REQUIRE(fromLiteral == copy);

    // Test decay to std::string_view
    std::string_view view = fromLiteral;
    REQUIRE(view == fromLiteral);

    // Test operator[]
    REQUIRE(fromLiteral[0] == 'a');
    REQUIRE(fromLiteral[1] == 'b');
    REQUIRE(fromLiteral[2] == 'c');
    REQUIRE(fromLiteral[3] == '\0');

    // Test constructing with no NULL in range
    static constexpr char badCharArray[2][3] = {{'a', 'b', 'c'}, {'a', 'b', 'c'}};
    REQUIRE_ERROR((wil::zstring_view{&badCharArray[0][0], _countof(badCharArray[0])}));
    REQUIRE_ERROR((wil::zstring_view{badCharArray[0]}));

    // Test constructing with a NULL one character past the valid range, guarding against off-by-one errors
    // Overloads taking an explicit length trust the user that they ensure valid memory follows the buffer
    static constexpr char badCharArrayOffByOne[2][3] = {{'a', 'b', 'c'}, {}};
    const wil::zstring_view fromTerminatedCharArray(&badCharArrayOffByOne[0][0], _countof(badCharArrayOffByOne[0]));
    REQUIRE(fromLiteral == fromTerminatedCharArray);
    REQUIRE_ERROR((wil::zstring_view{badCharArrayOffByOne[0]}));

    // Test constructing from custom string type
    CustomNoncopyableString customString;
    wil::zstring_view fromCustomString(customString);
    REQUIRE(fromCustomString == (PCSTR)customString);
}

TEST_CASE("StlTests::TestZWStringView literal", "[stl][zwstring_view]")
{

    SECTION("Literal creates correct zwstring_view")
    {
        auto str = L"Hello, world!"_zv;
        REQUIRE(str.length() == 13);
        REQUIRE(str[0] == L'H');
        REQUIRE(str[12] == L'!');
    }
}

TEST_CASE("StlTests::TestZStringView literal", "[stl][zstring_view]")
{

    SECTION("Literal creates correct zstring_view")
    {
        auto str = "Hello, world!"_zv;
        REQUIRE(str.length() == 13);
        REQUIRE(str[0] == 'H');
        REQUIRE(str[12] == '!');
    }
}

#if __cpp_lib_format >= 201907L

TEST_CASE("StlTests::TestZStringView formatting", "[stl][zstring_view]")
{
    SECTION("zstring_view can be used with std::format(wchar_t const*)")
    {
        auto str = L"kittens"_zv;
        auto fmtStr = std::format(L"Hello {}", str);
        REQUIRE(fmtStr == L"Hello kittens");
    }

    SECTION("zstring_view can be used with std::format(char const*)")
    {
        auto str = "kittens"_zv;
        auto fmtStr = std::format("Hello {}", str);
        REQUIRE(fmtStr == "Hello kittens");
    }
}

#endif

TEST_CASE("StlTests::TestZWStringView", "[stl][zstring_view]")
{
    // A default-constructed nullable view is empty and has data() == nullptr (matches the prior
    // single-class behaviour). The safe variant gets its own coverage in TestZWStringViewSafe
    // and in the templated matrix below.
    REQUIRE(wil::zwstring_view{}.empty());

    // Test empty string cases
    REQUIRE(wil::zwstring_view{L""}[0] == L'\0');
    REQUIRE(wil::zwstring_view{L""}.c_str()[0] == L'\0');
    REQUIRE(wil::zwstring_view{L""}.empty());

    // Test different constructor equality
    constexpr wil::zwstring_view fromLiteral = L"abc";
    REQUIRE(fromLiteral.length() == wcslen(L"abc"));

    std::wstring stlString = L"abc";
    wil::zwstring_view fromString(stlString);
    wil::zwstring_view fromPtr(stlString.data());

    static constexpr wchar_t charArray[] = L"abc";
    constexpr wil::zwstring_view fromArray(charArray);

    static constexpr wchar_t extendedCharArray[] = L"abc\0\0\0\0\0";
    constexpr wil::zwstring_view fromExtendedArray(extendedCharArray);

    wil::zwstring_view copy = fromLiteral;

    REQUIRE(fromLiteral == stlString);
    REQUIRE(fromLiteral == fromString);
    REQUIRE(fromLiteral == fromArray);
    REQUIRE(fromLiteral == fromExtendedArray);
    REQUIRE(fromLiteral == copy);

    // Test decay to std::wstring_view
    std::wstring_view view = fromLiteral;
    REQUIRE(view == fromLiteral);

    // Test operator[]
    REQUIRE(fromLiteral[0] == L'a');
    REQUIRE(fromLiteral[1] == L'b');
    REQUIRE(fromLiteral[2] == L'c');
    REQUIRE(fromLiteral[3] == L'\0');

    // Test constructing with no NULL in range
    static constexpr wchar_t badCharArray[2][3] = {{L'a', L'b', L'c'}, {L'a', L'b', L'c'}};
    REQUIRE_ERROR((wil::zwstring_view{&badCharArray[0][0], _countof(badCharArray[0])}));
    REQUIRE_ERROR((wil::zwstring_view{badCharArray[0]}));

    // Test constructing with a NULL one character past the valid range, guarding against off-by-one errors
    // Overloads taking an explicit length trust the user that they ensure valid memory follows the buffer
    static constexpr wchar_t badCharArrayOffByOne[2][3] = {{L'a', L'b', L'c'}, {}};
    const wil::zwstring_view fromTerminatedCharArray(&badCharArrayOffByOne[0][0], _countof(badCharArrayOffByOne[0]));
    REQUIRE(fromLiteral == fromTerminatedCharArray);
    REQUIRE_ERROR((wil::zwstring_view{badCharArrayOffByOne[0]}));

    // Test constructing from custom string type
    CustomNoncopyableString customString;
    wil::zwstring_view fromCustomString(customString);
    REQUIRE(fromCustomString == (PCWSTR)customString);

    // Test constructing from a type that has a c_str() method only
    struct string_with_c_str
    {
        using value_type = wchar_t;
        constexpr PCWSTR c_str() const
        {
            return L"hello";
        }
    };
    string_with_c_str fake_path{};
    REQUIRE(wil::zwstring_view(fake_path) == L"hello");
}

// Regression test for the substr(0) round-trip on a default-constructed nullable view. The
// substr(pos) override returns a basic_zstring_view by routing tail.data()/tail.size() through
// the trusted (ptr, length) ctor, which would deref a null pointer if substr(0) on a
// default-constructed nullable view (data() == nullptr) reached that ctor. The short-circuit
// in substr's body fires for the tail.data() == nullptr case, so the tail is itself a
// default-constructed view. The safe variant cannot hit this case (its default-constructed
// data() points at the static empty buffer), so the regression is nullable-only.

TEST_CASE("StlTests::TestZStringView default-constructed substr(0) round-trip is safe", "[stl][zstring_view]")
{
    wil::zstring_view zv;
    auto tail = zv.substr(0);
    REQUIRE(tail.empty());
    REQUIRE(tail.size() == 0);
    REQUIRE(tail.data() == nullptr);
    REQUIRE(tail.c_str() == nullptr);
}

TEST_CASE("StlTests::TestZWStringView default-constructed substr(0) round-trip is safe", "[stl][zstring_view]")
{
    wil::zwstring_view zv;
    auto tail = zv.substr(0);
    REQUIRE(tail.empty());
    REQUIRE(tail.size() == 0);
    REQUIRE(tail.data() == nullptr);
    REQUIRE(tail.c_str() == nullptr);
}

// Safe-variant default-construct contract. The safe variant (basic_zstring_view<TChar,
// zstring_view_traits_safe<TChar>>, aliased as wil::zstring_view_safe / wil::zwstring_view_safe)
// guarantees `data() != nullptr` on a default-constructed view: the view points at a static
// empty buffer so c_str() is always dereferenceable. These cases exercise c_str() use patterns
// (strlen, %s, std::string ctor, C-API handoff), observable equivalence with an empty-literal
// view, and defensive `if (zv.data())` guards. The templated matrix below repeats the same
// contract for both variants under a single body via `if constexpr (is_safe_zsv_v<ZSV>)`.

TEST_CASE("StlTests::TestZStringViewSafe default-construct safety", "[stl][zstring_view][safe]")
{
    SECTION("c_str()-style operations on a default-constructed view are well-defined")
    {
        wil::zstring_view_safe zv;

        // c_str() returns a dereferenceable pointer; first character is '\0'.
        REQUIRE(zv.c_str() != nullptr);
        REQUIRE(zv.c_str()[0] == '\0');
        REQUIRE(*zv.c_str() == '\0');

        // strlen and equivalent C APIs see an empty string rather than UB.
        REQUIRE(strlen(zv.c_str()) == 0);

        // Constructing a std::string from c_str() produces an empty string.
        REQUIRE(std::string(zv.c_str()).empty());
    }

    SECTION("container-style operations on a default-constructed view")
    {
        wil::zstring_view_safe zv;

        REQUIRE(zv.empty());
        REQUIRE(zv.size() == 0);
        REQUIRE(zv.length() == 0);

        // Decay to std::string_view via the safe variant's implicit conversion operator.
        std::string_view sv = zv;
        REQUIRE(sv.empty());
        REQUIRE(sv.size() == 0);

        // Self-equality and equality with an empty safe view.
        REQUIRE(zv == wil::zstring_view_safe{});

        // Iteration yields zero passes.
        int count = 0;
        for (auto c : zv)
        {
            (void)c;
            ++count;
        }
        REQUIRE(count == 0);
    }

    SECTION("default-constructed and empty-literal views are observably equivalent")
    {
        wil::zstring_view_safe defaulted;
        wil::zstring_view_safe fromEmptyLiteral{""};

        REQUIRE(defaulted.empty() == fromEmptyLiteral.empty());
        REQUIRE(defaulted.size() == fromEmptyLiteral.size());
        REQUIRE(strlen(defaulted.c_str()) == strlen(fromEmptyLiteral.c_str()));
        REQUIRE(defaulted == fromEmptyLiteral);
    }

    SECTION("defensive `if (zv.data())` guards compile and behave correctly")
    {
        wil::zstring_view_safe zv;

        // `if (zv.data())` is always true on a default-constructed safe view; the body
        // operates on a valid empty C-string.
        bool sawValidPointer = false;
        if (zv.data())
        {
            sawValidPointer = true;
            REQUIRE(strlen(zv.c_str()) == 0);
        }
        REQUIRE(sawValidPointer);

        // `if (!zv.empty())` skips the body for any empty view.
        bool sawNonEmpty = false;
        if (!zv.empty())
        {
            sawNonEmpty = true;
        }
        REQUIRE(!sawNonEmpty);
    }
}

TEST_CASE("StlTests::TestZWStringViewSafe default-construct safety", "[stl][zstring_view][safe]")
{
    SECTION("c_str()-style operations on a default-constructed view are well-defined")
    {
        wil::zwstring_view_safe zv;

        REQUIRE(zv.c_str() != nullptr);
        REQUIRE(zv.c_str()[0] == L'\0');
        REQUIRE(*zv.c_str() == L'\0');

        REQUIRE(wcslen(zv.c_str()) == 0);

        REQUIRE(std::wstring(zv.c_str()).empty());
    }

    SECTION("container-style operations on a default-constructed view")
    {
        wil::zwstring_view_safe zv;

        REQUIRE(zv.empty());
        REQUIRE(zv.size() == 0);
        REQUIRE(zv.length() == 0);

        std::wstring_view sv = zv;
        REQUIRE(sv.empty());
        REQUIRE(sv.size() == 0);

        REQUIRE(zv == wil::zwstring_view_safe{});

        int count = 0;
        for (auto c : zv)
        {
            (void)c;
            ++count;
        }
        REQUIRE(count == 0);
    }

    SECTION("default-constructed and empty-literal views are observably equivalent")
    {
        wil::zwstring_view_safe defaulted;
        wil::zwstring_view_safe fromEmptyLiteral{L""};

        REQUIRE(defaulted.empty() == fromEmptyLiteral.empty());
        REQUIRE(defaulted.size() == fromEmptyLiteral.size());
        REQUIRE(wcslen(defaulted.c_str()) == wcslen(fromEmptyLiteral.c_str()));
        REQUIRE(defaulted == fromEmptyLiteral);
    }

    SECTION("defensive `if (zv.data())` guards compile and behave correctly")
    {
        wil::zwstring_view_safe zv;

        bool sawValidPointer = false;
        if (zv.data())
        {
            sawValidPointer = true;
            REQUIRE(wcslen(zv.c_str()) == 0);
        }
        REQUIRE(sawValidPointer);

        bool sawNonEmpty = false;
        if (!zv.empty())
        {
            sawNonEmpty = true;
        }
        REQUIRE(!sawNonEmpty);
    }
}

// Templated tests using Catch2's TEMPLATE_TEST_CASE to share the same body across char
// and wchar_t. Tagged [templated] so they can be filtered separately. Covers the
// null-termination invariant, the substr split, the contains backport, safe default-construct
// semantics, and the read-only base-class interface as seen through the derived type.

namespace zsv_test_helpers
{
template <typename TChar>
struct strings;

template <>
struct strings<char>
{
    static constexpr const char* hello_world = "Hello, World!";
    static constexpr const char* hello = "Hello";
    static constexpr const char* world = "World!";
    static constexpr const char* empty = "";
    static constexpr const char* not_present = "xyz";
    static constexpr char test_char = 'o';
    static constexpr char absent_char = 'Z';
    // "abc" literal in each character type, used by the construction-matrix tests.
    static constexpr const char* abc = "abc";
};

template <>
struct strings<wchar_t>
{
    static constexpr const wchar_t* hello_world = L"Hello, World!";
    static constexpr const wchar_t* hello = L"Hello";
    static constexpr const wchar_t* world = L"World!";
    static constexpr const wchar_t* empty = L"";
    static constexpr const wchar_t* not_present = L"xyz";
    static constexpr wchar_t test_char = L'o';
    static constexpr wchar_t absent_char = L'Z';
    static constexpr const wchar_t* abc = L"abc";
};

// Asserts the core null-termination invariant. Templated on the ZSV instantiation so it
// accepts both the nullable (`wil::zstring_view` / `wil::zwstring_view`) and the safe
// (`wil::zstring_view_safe` / `wil::zwstring_view_safe`) variants without an implicit
// cross-traits conversion at the call site.
template <typename ZSV>
void assert_null_terminated(ZSV v)
{
    using TChar = typename ZSV::value_type;
    REQUIRE(v.data() != nullptr);
    REQUIRE(v.data()[v.size()] == TChar());
}

// Distinguishes the safe Traits variant from the default-traits (nullable) variant by
// inspecting the ZSV's `traits_type`. The safe variant's traits is
// `wil::zstring_view_traits_safe<TChar>` (derived from `std::char_traits<TChar>` but a
// distinct type); the nullable variant uses `std::char_traits<TChar>` directly.
template <typename ZSV>
constexpr bool is_safe_zsv_v = !std::is_same_v<typename ZSV::traits_type, std::char_traits<typename ZSV::value_type>>;

// Templated source type providing only c_str() (no size()) -- exercises the
// SFINAE constructor that accepts c_str()-only sources.
template <typename TChar>
struct string_with_c_str_only
{
    using value_type = TChar;
    constexpr const TChar* c_str() const
    {
        if constexpr (std::is_same_v<TChar, char>)
        {
            return "hello";
        }
        else
        {
            return L"hello";
        }
    }
};
} // namespace zsv_test_helpers

#define ZSV_TYPES char, wchar_t

// Four-way matrix: nullable + safe variants of both narrow and wide. Used by every templated
// test that should hold for either Traits choice. The two outliers that stay on `ZSV_TYPES`
// are the `_zv` literal and `std::format` blocks, because `_zv` always materialises the
// nullable variant (the safe-variant equivalent `_zvs` is covered by its own block).
#define ZSV_VARIANTS wil::zstring_view, wil::zwstring_view, wil::zstring_view_safe, wil::zwstring_view_safe

TEMPLATE_TEST_CASE("StlTests::ZStringView construction", "[stl][zstring_view][templated]", ZSV_VARIANTS)
{
    using ZSV = TestType;
    using TChar = typename ZSV::value_type;
    using S = zsv_test_helpers::strings<TChar>;

    SECTION("default and empty-literal views are both empty and read as null-terminated")
    {
        // Default-construct: both variants report empty(); the safe variant additionally
        // guarantees a dereferenceable null-terminated buffer, while the nullable variant
        // keeps the legacy nullptr-data behaviour.
        REQUIRE(ZSV{}.empty());
        if constexpr (zsv_test_helpers::is_safe_zsv_v<ZSV>)
        {
            REQUIRE(ZSV{}.data() != nullptr);
            REQUIRE(ZSV{}.c_str()[0] == TChar());
        }
        else
        {
            REQUIRE(ZSV{}.data() == nullptr);
            // c_str() is a thin alias for data() on the default variant; assert both to pin
            // down the contract in case c_str() ever grows independent logic.
            REQUIRE(ZSV{}.c_str() == nullptr);
        }
        // Empty literal: subscript reads '\0', c_str() reads '\0', empty().
        REQUIRE(ZSV{S::empty}[0] == TChar());
        REQUIRE(ZSV{S::empty}.c_str()[0] == TChar());
        REQUIRE(ZSV{S::empty}.empty());
    }

    SECTION("all ctor paths produce equivalent views")
    {
        // ctor from string literal (and length matches char_traits::length).
        ZSV fromLiteral = S::abc;
        REQUIRE(fromLiteral.length() == std::char_traits<TChar>::length(S::abc));

        // ctor from std::basic_string.
        std::basic_string<TChar> stlString = S::abc;
        ZSV fromString(stlString);
        // ctor from TChar* (pointer).
        ZSV fromPtr(stlString.data());
        // ctor from null-terminated array.
        static constexpr TChar charArray[] = {TChar('a'), TChar('b'), TChar('c'), TChar()};
        ZSV fromArray(charArray);
        // ctor from extended array (trailing zeros beyond the first null).
        static constexpr TChar extendedCharArray[] = {TChar('a'), TChar('b'), TChar('c'), TChar(), TChar(), TChar(), TChar(), TChar()};
        ZSV fromExtendedArray(extendedCharArray);
        // copy ctor.
        ZSV copy = fromLiteral;

        // The mixed-traits comparison (basic_string_view<TChar, safe_traits> vs
        // basic_string<TChar, std::char_traits>) has no matching operator==; route both sides
        // through std::basic_string_view<TChar> so the nullable variant slices to its
        // inherited base and the safe variant exercises its implicit conversion operator.
        REQUIRE(std::basic_string_view<TChar>{fromLiteral} == stlString);
        REQUIRE(fromLiteral == fromString);
        REQUIRE(fromLiteral == fromPtr);
        REQUIRE(fromLiteral == fromArray);
        REQUIRE(fromLiteral == fromExtendedArray);
        REQUIRE(fromLiteral == copy);
    }

    SECTION("decays to base string_view")
    {
        ZSV fromLiteral = S::abc;
        std::basic_string_view<TChar> view = fromLiteral;
        // Mixed-traits compare: see note in "all ctor paths produce equivalent views".
        REQUIRE(view == std::basic_string_view<TChar>{fromLiteral});
    }

    SECTION("operator[] reads each char including the trailing null")
    {
        ZSV fromLiteral = S::abc;
        REQUIRE(fromLiteral[0] == TChar('a'));
        REQUIRE(fromLiteral[1] == TChar('b'));
        REQUIRE(fromLiteral[2] == TChar('c'));
        // WIL's operator[] override allows reading view[size()] (the null terminator),
        // unlike base basic_string_view which forbids it.
        REQUIRE(fromLiteral[3] == TChar());
    }

    SECTION("fail-fast on no-NULL-in-range for explicit-length and array ctors")
    {
        static constexpr TChar badCharArray[2][3] = {{TChar('a'), TChar('b'), TChar('c')}, {TChar('a'), TChar('b'), TChar('c')}};
        REQUIRE_ERROR((ZSV{&badCharArray[0][0], _countof(badCharArray[0])}));
        REQUIRE_ERROR((ZSV{badCharArray[0]}));
    }

    SECTION("explicit-length ctor accepts the off-by-one valid boundary case")
    {
        ZSV fromLiteral = S::abc;
        static constexpr TChar badCharArrayOffByOne[2][3] = {{TChar('a'), TChar('b'), TChar('c')}, {}};
        const ZSV fromTerminatedCharArray(&badCharArrayOffByOne[0][0], _countof(badCharArrayOffByOne[0]));
        REQUIRE(fromLiteral == fromTerminatedCharArray);
        REQUIRE_ERROR((ZSV{badCharArrayOffByOne[0]}));
    }

    SECTION("ctor from a type implicitly convertible to a C-string pointer")
    {
        CustomNoncopyableString customString;
        ZSV fromCustomString(customString);
        if constexpr (std::is_same_v<TChar, char>)
        {
            REQUIRE(fromCustomString == static_cast<PCSTR>(customString));
        }
        else
        {
            REQUIRE(fromCustomString == static_cast<PCWSTR>(customString));
        }
    }

    SECTION("ctor from a type with only c_str() (no size())")
    {
        zsv_test_helpers::string_with_c_str_only<TChar> fake_path{};
        ZSV view(fake_path);
        if constexpr (std::is_same_v<TChar, char>)
        {
            REQUIRE(view == "hello");
        }
        else
        {
            REQUIRE(view == L"hello");
        }
    }
}

TEMPLATE_TEST_CASE("StlTests::ZStringView _zv literal", "[stl][zstring_view][templated]", ZSV_TYPES)
{
    using TChar = TestType;

    SECTION("_zv literal preserves length and content")
    {
        if constexpr (std::is_same_v<TChar, char>)
        {
            auto str = "Hello, world!"_zv;
            STATIC_REQUIRE(std::is_same_v<decltype(str), wil::zstring_view>);
            REQUIRE(str.length() == 13);
            REQUIRE(str[0] == 'H');
            REQUIRE(str[12] == '!');
        }
        else
        {
            auto str = L"Hello, world!"_zv;
            STATIC_REQUIRE(std::is_same_v<decltype(str), wil::zwstring_view>);
            REQUIRE(str.length() == 13);
            REQUIRE(str[0] == L'H');
            REQUIRE(str[12] == L'!');
        }
    }
}

// Parity block for the safe-variant literal. Mirrors the `_zv literal` test above so we keep
// the matrix-style coverage symmetric across both UDLs; the decltype assertion is the
// load-bearing piece - it pins down which alias the literal materialises.
TEMPLATE_TEST_CASE("StlTests::ZStringView _zvs literal", "[stl][zstring_view][templated]", ZSV_TYPES)
{
    using TChar = TestType;

    SECTION("_zvs literal preserves length and content and materialises the safe variant")
    {
        if constexpr (std::is_same_v<TChar, char>)
        {
            auto str = "Hello, world!"_zvs;
            STATIC_REQUIRE(std::is_same_v<decltype(str), wil::zstring_view_safe>);
            REQUIRE(str.length() == 13);
            REQUIRE(str[0] == 'H');
            REQUIRE(str[12] == '!');
        }
        else
        {
            auto str = L"Hello, world!"_zvs;
            STATIC_REQUIRE(std::is_same_v<decltype(str), wil::zwstring_view_safe>);
            REQUIRE(str.length() == 13);
            REQUIRE(str[0] == L'H');
            REQUIRE(str[12] == L'!');
        }
    }
}

#if __cpp_lib_format >= 201907L
TEMPLATE_TEST_CASE("StlTests::ZStringView std::format support", "[stl][zstring_view][templated]", ZSV_TYPES)
{
    using TChar = TestType;

    SECTION("round-trips through std::format")
    {
        if constexpr (std::is_same_v<TChar, char>)
        {
            auto str = "kittens"_zv;
            auto fmtStr = std::format("Hello {}", str);
            REQUIRE(fmtStr == "Hello kittens");
        }
        else
        {
            auto str = L"kittens"_zv;
            auto fmtStr = std::format(L"Hello {}", str);
            REQUIRE(fmtStr == L"Hello kittens");
        }
    }
}
#endif // __cpp_lib_format >= 201907L

TEST_CASE("StlTests::ZStringView type aliases", "[stl][zstring_view]")
{
    // Nullable variant: default-traits aliases and `_zv` UDL bindings.
    STATIC_REQUIRE(std::is_same_v<wil::zstring_view, wil::basic_zstring_view<char>>);
    STATIC_REQUIRE(std::is_same_v<wil::zwstring_view, wil::basic_zstring_view<wchar_t>>);
    STATIC_REQUIRE(std::is_same_v<decltype("hello"_zv), wil::zstring_view>);
    STATIC_REQUIRE(std::is_same_v<decltype(L"hello"_zv), wil::zwstring_view>);

    // Safe variant: opt-in Traits aliases and `_zvs` UDL bindings.
    STATIC_REQUIRE(std::is_same_v<wil::zstring_view_safe, wil::basic_zstring_view<char, wil::zstring_view_traits_safe<char>>>);
    STATIC_REQUIRE(std::is_same_v<wil::zwstring_view_safe, wil::basic_zstring_view<wchar_t, wil::zstring_view_traits_safe<wchar_t>>>);
    STATIC_REQUIRE(std::is_same_v<decltype("hello"_zvs), wil::zstring_view_safe>);
    STATIC_REQUIRE(std::is_same_v<decltype(L"hello"_zvs), wil::zwstring_view_safe>);

    // The two variants are distinct types: the Traits parameter changes the instantiation, so
    // there is no implicit ABI/identity collapse between safe and nullable.
    STATIC_REQUIRE(!std::is_same_v<wil::zstring_view, wil::zstring_view_safe>);
    STATIC_REQUIRE(!std::is_same_v<wil::zwstring_view, wil::zwstring_view_safe>);
}

// Pins down the behavioural delta between the safe and nullable variants in one place.
// Anything that *should* differ between the two Traits choices lives here, so a future change
// that accidentally collapses the two will trip a focused, well-named failure.
TEST_CASE("StlTests::ZStringView safe-variant invariants", "[stl][zstring_view][safe]")
{
    SECTION("safe variant rejects nullptr_t construction at compile time")
    {
        // The safe variant SFINAE-deletes its `nullptr_t` constructor; the nullable variant
        // accepts `nullptr` via its inherited base. Both are intentional.
        STATIC_REQUIRE(!std::is_constructible_v<wil::zstring_view_safe, std::nullptr_t>);
        STATIC_REQUIRE(!std::is_constructible_v<wil::zwstring_view_safe, std::nullptr_t>);
        STATIC_REQUIRE(std::is_constructible_v<wil::zstring_view, std::nullptr_t>);
        STATIC_REQUIRE(std::is_constructible_v<wil::zwstring_view, std::nullptr_t>);
    }

    SECTION("safe variant fail-fasts on (nullptr, 0) at runtime")
    {
        // The safe variant's `(ptr, len)` constructor routes through `_require_non_null` and
        // fail-fasts when the pointer is null, *even* for zero-length. The nullable variant
        // is deliberately not exercised here: its `(ptr, len)` ctor unconditionally reads
        // `pStringData[stringLength]` and therefore has latent UB when called with
        // `(nullptr, 0)`. Documenting that gap rather than triggering it.
        REQUIRE_ERROR((wil::zstring_view_safe{nullptr, 0}));
        REQUIRE_ERROR((wil::zwstring_view_safe{nullptr, 0}));
    }

    SECTION("safe variant's default-constructed view has a non-null buffer")
    {
        // The whole point of the safe Traits: the default-constructed view dereferences to a
        // valid empty C-string instead of nullptr. Mirrors the templated `construction` block
        // but kept here as an at-a-glance, narrative-style guarantee.
        constexpr wil::zstring_view_safe narrow;
        constexpr wil::zwstring_view_safe wide;
        STATIC_REQUIRE(narrow.data() != nullptr);
        STATIC_REQUIRE(wide.data() != nullptr);
        STATIC_REQUIRE(narrow.empty());
        STATIC_REQUIRE(wide.empty());
        REQUIRE(narrow.c_str()[0] == '\0');
        REQUIRE(wide.c_str()[0] == L'\0');
    }
}

TEMPLATE_TEST_CASE("StlTests::ZStringView element access (data/c_str identity)", "[stl][zstring_view][templated]", ZSV_VARIANTS)
{
    using ZSV = TestType;
    using TChar = typename ZSV::value_type;
    using S = zsv_test_helpers::strings<TChar>;

    ZSV zv = S::hello_world;

    SECTION("data() and c_str() return the same pointer and are null-terminated")
    {
        REQUIRE(zv.data() == zv.c_str());
        REQUIRE(zv.c_str()[zv.size()] == TChar());
    }
}

#if defined(__cpp_lib_starts_ends_with) && __cpp_lib_starts_ends_with >= 201711L
TEMPLATE_TEST_CASE("StlTests::ZStringView starts_with and ends_with", "[stl][zstring_view][templated]", ZSV_VARIANTS)
{
    using ZSV = TestType;
    using TChar = typename ZSV::value_type;
    using S = zsv_test_helpers::strings<TChar>;

    ZSV zv = S::hello_world;

    SECTION("starts_with matches a known prefix")
    {
        REQUIRE(zv.starts_with(S::hello));
        REQUIRE(!zv.starts_with(S::world));
        REQUIRE(zv.starts_with(S::hello_world[0])); // single char
    }

    SECTION("ends_with matches a known suffix")
    {
        REQUIRE(zv.ends_with(S::world));
        REQUIRE(!zv.ends_with(S::hello));
    }

    SECTION("empty pattern always starts_with and ends_with anything")
    {
        REQUIRE(zv.starts_with(S::empty));
        REQUIRE(zv.ends_with(S::empty));
    }
}
#endif // __cpp_lib_starts_ends_with

TEMPLATE_TEST_CASE("StlTests::ZStringView contains", "[stl][zstring_view][templated]", ZSV_VARIANTS)
{
    using ZSV = TestType;
    using TChar = typename ZSV::value_type;
    using S = zsv_test_helpers::strings<TChar>;

    ZSV zv = S::hello_world;

    SECTION("contains(string_view) finds embedded substrings")
    {
        REQUIRE(zv.contains(ZSV(S::hello)));
        REQUIRE(zv.contains(ZSV(S::world)));
        REQUIRE(!zv.contains(ZSV(S::not_present)));
    }

    SECTION("contains(char) finds a present character")
    {
        REQUIRE(zv.contains(S::test_char));
        REQUIRE(!zv.contains(S::absent_char));
    }

    SECTION("contains(const TChar*) finds embedded substrings via raw pointer")
    {
        REQUIRE(zv.contains(S::hello));
        REQUIRE(!zv.contains(S::not_present));
    }

    SECTION("empty view contains nothing except the empty substring")
    {
        ZSV empty;
        REQUIRE(!empty.contains(S::test_char));
        REQUIRE(!empty.contains(S::hello));
        REQUIRE(empty.contains(ZSV()));
    }

    SECTION("contains return type is bool")
    {
        STATIC_REQUIRE(std::is_same_v<decltype(zv.contains(S::test_char)), bool>);
        STATIC_REQUIRE(std::is_same_v<decltype(zv.contains(S::hello)), bool>);
        STATIC_REQUIRE(std::is_same_v<decltype(zv.contains(ZSV(S::hello))), bool>);
    }
}

TEMPLATE_TEST_CASE("StlTests::ZStringView substr(pos) is covariant and preserves null-termination", "[stl][zstring_view][templated][substr]", ZSV_VARIANTS)
{
    using ZSV = TestType;
    using TChar = typename ZSV::value_type;
    using S = zsv_test_helpers::strings<TChar>;

    ZSV zv = S::hello_world;
    const std::size_t n = std::char_traits<TChar>::length(S::hello_world);

    SECTION("substr(pos) returns a basic_zstring_view (covariant return type)")
    {
        STATIC_REQUIRE(std::is_same_v<decltype(zv.substr(0)), ZSV>);
        STATIC_REQUIRE(std::is_same_v<decltype(zv.substr()), ZSV>);
    }

    SECTION("substr(pos) returns the null-terminated tail")
    {
        auto tail = zv.substr(7);
        REQUIRE(tail.size() == n - 7);
        REQUIRE(tail == ZSV(S::world));
        zsv_test_helpers::assert_null_terminated(tail);
    }

    SECTION("substr(0) returns a copy of the whole view")
    {
        auto whole = zv.substr(0);
        REQUIRE(whole == zv);
        zsv_test_helpers::assert_null_terminated(whole);
    }

    SECTION("substr(size()) returns an empty view at the trailing null")
    {
        auto empty = zv.substr(n);
        REQUIRE(empty.empty());
        REQUIRE(empty.size() == 0);
        zsv_test_helpers::assert_null_terminated(empty);
    }

    SECTION("substr(pos > size()) throws out_of_range, propagating from base")
    {
        REQUIRE_THROWS_AS(zv.substr(n + 1), std::out_of_range);
    }

    SECTION("substr chaining preserves null-termination at each step")
    {
        auto step1 = zv.substr(0);    // "Hello, World!"
        auto step2 = step1.substr(7); // "World!"
        auto step3 = step2.substr(0); // "World!"
        zsv_test_helpers::assert_null_terminated(step1);
        zsv_test_helpers::assert_null_terminated(step2);
        zsv_test_helpers::assert_null_terminated(step3);
        REQUIRE(step3 == ZSV(S::world));
    }
}

TEMPLATE_TEST_CASE("StlTests::ZStringView substr(pos, count) returns string_view", "[stl][zstring_view][templated][substr]", ZSV_VARIANTS)
{
    using ZSV = TestType;
    using TChar = typename ZSV::value_type;
    // `substr(pos, count)` is the inherited base override, so it returns the base's
    // own-traits string_view -- not necessarily `std::basic_string_view<TChar>` with default
    // traits. For the safe variant the base is `basic_string_view<TChar, safe_traits>`.
    using SV = std::basic_string_view<TChar, typename ZSV::traits_type>;
    using S = zsv_test_helpers::strings<TChar>;

    ZSV zv = S::hello_world;

    SECTION("substr(pos, count) returns a std::basic_string_view, not a basic_zstring_view")
    {
        STATIC_REQUIRE(std::is_same_v<decltype(zv.substr(0, 5)), SV>);
        STATIC_REQUIRE(std::is_same_v<decltype(zv.substr(7, 5)), SV>);
    }

    SECTION("substr(0, prefix_len) returns the prefix as a string_view")
    {
        auto prefix = zv.substr(0, 5);
        REQUIRE(prefix.size() == 5);
        REQUIRE(prefix == SV(S::hello));
    }

    SECTION("substr(pos, count) clamps count to size() - pos")
    {
        auto slice = zv.substr(7, 1000);
        REQUIRE(slice.size() == std::char_traits<TChar>::length(S::hello_world) - 7);
    }

    SECTION("substr(pos, 0) returns an empty string_view")
    {
        auto empty = zv.substr(3, 0);
        REQUIRE(empty.empty());
        REQUIRE(empty.size() == 0);
    }

    SECTION("substr(size(), count) returns an empty string_view at the trailing null")
    {
        const auto n = zv.size();
        auto empty = zv.substr(n, 5);
        REQUIRE(empty.empty());
    }
}

TEMPLATE_TEST_CASE("StlTests::ZStringView remove_prefix preserves null-termination", "[stl][zstring_view][templated]", ZSV_VARIANTS)
{
    using ZSV = TestType;
    using TChar = typename ZSV::value_type;
    using S = zsv_test_helpers::strings<TChar>;

    SECTION("after remove_prefix the view remains null-terminated and c_str() == data()")
    {
        ZSV zv = S::hello_world;
        zv.remove_prefix(7);
        zsv_test_helpers::assert_null_terminated(zv);
        REQUIRE(zv.c_str() == zv.data());
        REQUIRE(zv.c_str()[zv.size()] == TChar());
    }
}

TEMPLATE_TEST_CASE("StlTests::ZStringView conversion to string_view", "[stl][zstring_view][templated]", ZSV_VARIANTS)
{
    using ZSV = TestType;
    using TChar = typename ZSV::value_type;
    // Deliberately default-traits `std::basic_string_view` here: the test asserts both
    // variants interoperate with the broad ecosystem of APIs that take the stdlib's
    // default-traits string_view. The nullable variant gets this via inherited-base
    // slicing; the safe variant uses its implicit `operator std::basic_string_view<TChar>()`.
    using SV = std::basic_string_view<TChar>;
    using S = zsv_test_helpers::strings<TChar>;

    SECTION("implicit conversion to string_view preserves size and data")
    {
        ZSV zv = S::hello_world;
        SV sv = zv;
        REQUIRE(sv.size() == zv.size());
        REQUIRE(sv.data() == zv.data());
    }

    SECTION("can pass a basic_zstring_view to a function expecting basic_string_view")
    {
        auto take_sv = [](SV sv) {
            return sv.size();
        };
        ZSV zv = S::hello_world;
        REQUIRE(take_sv(zv) == zv.size());
    }

    SECTION("can construct a std::basic_string from a basic_zstring_view")
    {
        ZSV zv = S::hello_world;
        std::basic_string<TChar> str(zv);
        REQUIRE(str.size() == zv.size());
        // Mixed-traits compare: route through default-traits string_view so the safe variant's
        // basic_string_view<TChar, safe_traits> doesn't break the inherited operator== match.
        REQUIRE(str == std::basic_string_view<TChar>{zv});
    }
}

TEMPLATE_TEST_CASE("StlTests::ZStringView cross-variant constructors preserve data and size", "[stl][zstring_view][templated]", ZSV_TYPES)
{
    using TChar = TestType;
    using Safe = wil::basic_zstring_view<TChar, wil::zstring_view_traits_safe<TChar>>;
    using Default = wil::basic_zstring_view<TChar>;
    using S = zsv_test_helpers::strings<TChar>;

    SECTION("implicit ctor: a non-empty safe view widens to a default view sharing data and size")
    {
        Safe safe = S::hello_world;
        Default def = safe;
        REQUIRE(def.data() == safe.data());
        REQUIRE(def.size() == safe.size());
        REQUIRE(def.c_str() == safe.c_str());
    }

    SECTION("implicit ctor: a default-constructed safe view widens to a default view with non-null data")
    {
        Safe safe;
        Default def = safe;
        REQUIRE(def.data() != nullptr);
        REQUIRE(def.size() == 0);
        REQUIRE(def.empty());
    }

    SECTION("explicit ctor: an engaged default view narrows to a safe view sharing data and size")
    {
        Default def = S::hello_world;
        Safe safe{def};
        REQUIRE(safe.data() == def.data());
        REQUIRE(safe.size() == def.size());
    }
}

TEMPLATE_TEST_CASE("StlTests::ZStringView safe-variant disambiguates between zstring_view and string_view overloads", "[stl][zstring_view][templated]", ZSV_TYPES)
{
    using TChar = TestType;
    using Safe = wil::basic_zstring_view<TChar, wil::zstring_view_traits_safe<TChar>>;
    using S = zsv_test_helpers::strings<TChar>;

    // Pin down the disambiguation idiom from the class doc-block's overload-resolution caveat.
    // A call site that has both `f(wil::zstring_view)` and `f(std::string_view)` overloads cannot
    // accept a `zstring_view_safe` directly (ambiguous: both are single user-defined conversions),
    // but either explicit wrap form resolves cleanly.

    enum class which
    {
        default_variant,
        default_string_view,
    };

    struct overloads
    {
        static which f(wil::basic_zstring_view<TChar>) noexcept
        {
            return which::default_variant;
        }
        static which f(std::basic_string_view<TChar>) noexcept
        {
            return which::default_string_view;
        }
    };

    Safe safe = S::hello_world;

    REQUIRE(overloads::f(wil::basic_zstring_view<TChar>{safe}) == which::default_variant);
    REQUIRE(overloads::f(std::basic_string_view<TChar>{safe}) == which::default_string_view);
}

TEMPLATE_TEST_CASE("StlTests::ZStringView constexpr usage", "[stl][zstring_view][templated]", ZSV_VARIANTS)
{
    using ZSV = TestType;
    using TChar = typename ZSV::value_type;
    using S = zsv_test_helpers::strings<TChar>;

    SECTION("default ctor, copy ctor, and basic accessors are constexpr-usable")
    {
        constexpr ZSV defaulted;
        STATIC_REQUIRE(defaulted.empty());
        STATIC_REQUIRE(defaulted.size() == 0);
        if constexpr (zsv_test_helpers::is_safe_zsv_v<ZSV>)
        {
            STATIC_REQUIRE(defaulted.data() != nullptr);
        }
        else
        {
            STATIC_REQUIRE(defaulted.data() == nullptr);
        }
    }

    SECTION("substr(pos) is constexpr-usable when constructed from a literal")
    {
        constexpr ZSV zv = S::hello_world;
        constexpr auto tail = zv.substr(7);
        STATIC_REQUIRE(tail.size() == std::char_traits<TChar>::length(S::hello_world) - 7);
    }
}

#endif
