#include "pch.h"

#include <wil/path.h>

#ifdef WIL_ENABLE_EXCEPTIONS
#include <filesystem>
#include <string>

using namespace std::literals;
#endif

#include "common.h"

template <typename PathType>
static void DoStringViewLikeConstructionTest()
{
    PathType pathDefault;
    REQUIRE(pathDefault.data() == nullptr);
    REQUIRE(pathDefault.size() == 0);
    REQUIRE(pathDefault.length() == 0);
    REQUIRE(pathDefault.empty());

    PathType pathNull(nullptr);
    REQUIRE(pathNull.data() == nullptr);
    REQUIRE(pathNull.size() == 0);
    REQUIRE(pathNull.length() == 0);
    REQUIRE(pathNull.empty());

    const wchar_t* const nullCstr = nullptr;
    PathType pathNullCstr(nullCstr);
    REQUIRE(pathNullCstr.data() == nullptr);
    REQUIRE(pathNullCstr.size() == 0);
    REQUIRE(pathNullCstr.length() == 0);
    REQUIRE(pathNullCstr.empty());

    const wchar_t* const cstr = L"C:/foo/bar";
    PathType pathCstr(cstr);
    REQUIRE(pathCstr.data() == cstr);
    REQUIRE(pathCstr.size() == 10);
    REQUIRE(pathCstr.length() == 10);
    REQUIRE(!pathCstr.empty());

    REQUIRE(!std::is_constructible_v<PathType, const char*>); // Can't compile with multi-byte string

    PathType pathSubView(cstr, 6);
    REQUIRE(pathSubView.data() == cstr);
    REQUIRE(pathSubView.size() == 6);
    REQUIRE(pathSubView.length() == 6);
    REQUIRE(!pathSubView.empty());

#ifdef WIL_ENABLE_EXCEPTIONS
    const std::wstring_view stringView(cstr);
    PathType pathStringView(stringView);
    REQUIRE(pathStringView.data() == cstr);
    REQUIRE(pathStringView.size() == 10);
    REQUIRE(pathStringView.length() == 10);
    REQUIRE(!pathStringView.empty());

    PathType pathStringViewRvalue(std::wstring_view{cstr});
    REQUIRE(pathStringViewRvalue.data() == cstr);
    REQUIRE(pathStringViewRvalue.size() == 10);
    REQUIRE(pathStringViewRvalue.length() == 10);
    REQUIRE(!pathStringViewRvalue.empty());

    REQUIRE(!std::is_constructible_v<PathType, std::string_view>); // Can't compile with multi-byte string

    const std::wstring string(cstr);
    PathType pathString(string);
    REQUIRE(pathString.data() == string.c_str());
    REQUIRE(pathString.size() == 10);
    REQUIRE(pathString.length() == 10);
    REQUIRE(!pathString.empty());

    REQUIRE(!std::is_constructible_v<PathType, std::wstring&&>); // Can't compile with an r-value std::wstring
    // PathType invalid(std::wstring{cstr}); // ERROR
    REQUIRE(!std::is_constructible_v<PathType, std::string>); // Can't compile with multi-byte string
#endif
}

TEST_CASE("PathViewTests::PathViewConstruction", "[path]")
{
    DoStringViewLikeConstructionTest<wil::path_view_nothrow>();
    DoStringViewLikeConstructionTest<wil::path_view_failfast>();
#ifdef WIL_ENABLE_EXCEPTIONS
    DoStringViewLikeConstructionTest<wil::path_view>();
#endif
}

template <typename PathType>
static void DoStringViewLikeAccessorTests()
{
    // NOTE: operator[], front(), and back() are only checked by an assert, so we can't test out of bounds accesses here
    PathType path(L"abcd");
    REQUIRE(path[0] == L'a');
    REQUIRE(path[1] == L'b');
    REQUIRE(path[2] == L'c');
    REQUIRE(path[3] == L'd');

    if constexpr (wistd::is_same_v<typename PathType::result_type, void>)
    {
        REQUIRE(path.at(0) == L'a');
        REQUIRE(path.at(1) == L'b');
        REQUIRE(path.at(2) == L'c');
        REQUIRE(path.at(3) == L'd');
        REQUIRE_ERROR(path.at(4));
        REQUIRE_ERROR(path.at(PathType::npos));
    }

    REQUIRE(path.front() == 'a');
    REQUIRE(path.back() == 'd');

    PathType unicode(L"👋🌎"); // Actually 4 characters
    REQUIRE(unicode[0] == 0xD83D);
    REQUIRE(unicode[1] == 0xDC4B);
    REQUIRE(unicode[2] == 0xD83C);
    REQUIRE(unicode[3] == 0xDF0E);

    if constexpr (wistd::is_same_v<typename PathType::result_type, void>)
    {
        REQUIRE(unicode.at(0) == 0xD83D);
        REQUIRE(unicode.at(1) == 0xDC4B);
        REQUIRE(unicode.at(2) == 0xD83C);
        REQUIRE(unicode.at(3) == 0xDF0E);
        REQUIRE_ERROR(unicode.at(4));
        REQUIRE_ERROR(unicode.at(PathType::npos));
    }

    REQUIRE(unicode.front() == 0xD83D);
    REQUIRE(unicode.back() == 0xDF0E);

    if constexpr (wistd::is_same_v<typename PathType::result_type, void>)
    {
        // NOTE: 'REQUIRE_ERROR' assumes that it's safe to continue execution after the point of failure. Therefore, we cannot use
        // a null pointer here since continued execution would lead to a null dereference
        PathType empty(L"");
        REQUIRE_ERROR(empty.at(0));
        REQUIRE_ERROR(empty.at(PathType::npos));
    }
}

TEST_CASE("PathViewTests::Accessors", "[path]")
{
    DoStringViewLikeAccessorTests<wil::path_view_nothrow>();
    REQUIRE(noexcept(wistd::declval<wil::path_view_nothrow>().at(0)));
    DoStringViewLikeAccessorTests<wil::path_view_failfast>();
    REQUIRE(noexcept(wistd::declval<wil::path_view_failfast>().at(0)));
#ifdef WIL_ENABLE_EXCEPTIONS
    DoStringViewLikeAccessorTests<wil::path_view>();
    REQUIRE(!noexcept(wistd::declval<wil::path_view>().at(0)));
#endif
}

template <typename PathType>
static void DoStringViewLikeSubStringTests()
{
    auto eval = [](auto&& callback)
    {
        PathType path(L"abcdefg"); // Length = 7;
        callback(path);
    };

    // NOTE: The 'remove_' functions assume the length to remove is valid
    eval([](PathType& path) {
        path.remove_prefix(0);
        REQUIRE(path == L"abcdefg");
    });
    eval([](PathType& path) {
        path.remove_prefix(1);
        REQUIRE(path == L"bcdefg");
    });
    eval([](PathType& path) {
        path.remove_prefix(4);
        REQUIRE(path == L"efg");
    });
    eval([](PathType& path) {
        path.remove_prefix(7);
        REQUIRE(path.empty());
    });

    eval([](PathType& path) {
        path.remove_suffix(0);
        REQUIRE(path == L"abcdefg");
    });
    eval([](PathType& path) {
        path.remove_suffix(1);
        REQUIRE(path == L"abcdef");
    });
    eval([](PathType& path) {
        path.remove_suffix(4);
        REQUIRE(path == L"abc");
    });
    eval([](PathType& path) {
        path.remove_suffix(7);
        REQUIRE(path.empty());
    });

    eval([](PathType& path) {
        REQUIRE(path.substr() == L"abcdefg");
        REQUIRE(path.substr(1) == L"bcdefg");
        REQUIRE(path.substr(0, 6) == L"abcdef");
        REQUIRE(path.substr(2, 3) == L"cde");
        REQUIRE(path.substr(3, 0).empty());
        REQUIRE(path.substr(7).empty());
        REQUIRE(path.substr(100).empty());
    });
}

TEST_CASE("PathViewTests::SubString", "[path]")
{
    DoStringViewLikeSubStringTests<wil::path_view_nothrow>();
    DoStringViewLikeSubStringTests<wil::path_view_failfast>();
#ifdef WIL_ENABLE_EXCEPTIONS
    DoStringViewLikeSubStringTests<wil::path_view>();
#endif
}

template <typename PathType>
static void DoStringViewLikeCopyTests()
{
    wchar_t buffer[7];

    PathType path(L"abcdefg");

    REQUIRE(path.copy(buffer, 7) == 7);
    REQUIRE(::wcsncmp(buffer, L"abcdefg", 7) == 0);

    ::wmemset(buffer, 0, ARRAYSIZE(buffer));
    REQUIRE(path.copy(buffer, 100) == 7);
    REQUIRE(::wcsncmp(buffer, L"abcdefg", 7) == 0);

    ::wmemset(buffer, 0, ARRAYSIZE(buffer));
    REQUIRE(path.copy(buffer, 0) == 0);

    ::wmemset(buffer, 0, ARRAYSIZE(buffer));
    REQUIRE(path.copy(buffer, 7, 1) == 6);
    REQUIRE(::wcsncmp(buffer, L"bcdefg", 6) == 0);

    ::wmemset(buffer, 0, ARRAYSIZE(buffer));
    REQUIRE(path.copy(buffer, 3, 2) == 3);
    REQUIRE(::wcsncmp(buffer, L"cde", 7) == 0);

    ::wmemset(buffer, 0, ARRAYSIZE(buffer));
    REQUIRE(path.copy(buffer, 7, 7) == 0);

    ::wmemset(buffer, 0, ARRAYSIZE(buffer));
    REQUIRE(path.copy(buffer, 7, 100) == 0);

    PathType empty;
    REQUIRE(empty.copy(buffer, 7) == 0);
    REQUIRE(empty.copy(buffer, 7, 100) == 0);
}

TEST_CASE("PathViewTests::Copy", "[path]")
{
    DoStringViewLikeCopyTests<wil::path_view_nothrow>();
    DoStringViewLikeCopyTests<wil::path_view_failfast>();
#ifdef WIL_ENABLE_EXCEPTIONS
    DoStringViewLikeCopyTests<wil::path_view>();
#endif
}

template <typename PathType>
static void DoStringViewLikeComparisonTests()
{
    auto evalCompare = [](PathType& path, PathType compare, int expect) {
        auto checkSign = [](int value, int expected) {
            if (expected < 0) REQUIRE(value < 0);
            else if (expected > 0) REQUIRE(value > 0);
            else REQUIRE(value == 0);
        };

        checkSign(path.compare(compare), expect);
        checkSign(path.compare(compare.data()), expect);
#ifdef WIL_ENABLE_EXCEPTIONS
        checkSign(path.compare(std::wstring(compare.data(), compare.size())), expect);
        checkSign(path.compare(std::wstring_view(compare.data(), compare.size())), expect);
#endif
    };

    PathType path(L"abcdefg");
    REQUIRE(path.compare(path) == 0);
    evalCompare(path, L"abcdefg", 0);
    evalCompare(path, L"abcdef", 1);
    evalCompare(path, L"abcdefgh", -1);
    evalCompare(path, L"aaaaaaa", 1);
    evalCompare(path, L"bbbbbbb", -1);
    evalCompare(path, PathType(), 1);
    REQUIRE(path.compare(PathType(L"abcdefg", 8)) < 0);
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(path.compare(L"abcdefg\0"s) < 0);
    REQUIRE(path.compare(L"abcdefg\0"sv) < 0);
#endif

    auto evalStartsWith = [](PathType& path, PathType compare, bool expect) {
        REQUIRE(path.starts_with(compare) == expect);
        REQUIRE(path.starts_with(compare.data()) == expect);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(path.starts_with(std::wstring(compare.data(), compare.size())) == expect);
        REQUIRE(path.starts_with(std::wstring_view(compare.data(), compare.size())) == expect);
#endif
    };

    REQUIRE(path.starts_with(L'a'));
    REQUIRE(!path.starts_with(L'b'));
    evalStartsWith(path, PathType(), true);
    evalStartsWith(path, L"a", true);
    evalStartsWith(path, L"abc", true);
    evalStartsWith(path, L"abcdefg", true);
    evalStartsWith(path, L"b", false);
    evalStartsWith(path, L"abcdefgh", false);
    REQUIRE(!path.starts_with(PathType(L"abcdefg", 8)));
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(!path.starts_with(L"abcdefg\0"s));
    REQUIRE(!path.starts_with(L"abcdefg\0"sv));
#endif

    auto evalEndsWith = [](PathType& path, PathType compare, bool expect) {
        REQUIRE(path.ends_with(compare) == expect);
        REQUIRE(path.ends_with(compare.data()) == expect);
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(path.ends_with(std::wstring(compare.data(), compare.size())) == expect);
        REQUIRE(path.ends_with(std::wstring_view(compare.data(), compare.size())) == expect);
#endif
    };

    REQUIRE(path.ends_with(L'g'));
    REQUIRE(!path.ends_with(L'f'));
    evalEndsWith(path, PathType(), true);
    evalEndsWith(path, L"g", true);
    evalEndsWith(path, L"efg", true);
    evalEndsWith(path, L"abcdefg", true);
    evalEndsWith(path, L"f", false);
    evalEndsWith(path, L"abcdefgh", false);
    REQUIRE(!path.ends_with(PathType(L"abcdefg", 8)));
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(!path.ends_with(L"abcdefg\0"s));
    REQUIRE(!path.ends_with(L"abcdefg\0"sv));
#endif

    // Should be able to compare with any type
    REQUIRE(path.compare(wil::path_view_nothrow(L"abcdefg")) == 0);
    REQUIRE(path.starts_with(wil::path_view_nothrow(L"abc")));
    REQUIRE(path.ends_with(wil::path_view_nothrow(L"efg")));
    REQUIRE(path.compare(wil::path_view_failfast(L"abcdefg")) == 0);
    REQUIRE(path.starts_with(wil::path_view_failfast(L"abc")));
    REQUIRE(path.ends_with(wil::path_view_failfast(L"efg")));
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(path.compare(wil::path_view(L"abcdefg")) == 0);
    REQUIRE(path.starts_with(wil::path_view(L"abc")));
    REQUIRE(path.ends_with(wil::path_view(L"efg")));
#endif

    PathType empty;
    REQUIRE(empty.compare(empty) == 0);
    evalCompare(empty, path, -1);
    evalCompare(empty, PathType(), 0);

    REQUIRE(!empty.starts_with(L'\0'));
    evalStartsWith(empty, PathType(), true);
    evalStartsWith(empty, L"a", false);
    REQUIRE(!empty.starts_with(PathType(L"", 1)));
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(!empty.starts_with(L"\0"s));
    REQUIRE(!empty.starts_with(L"\0"sv));
#endif

    REQUIRE(!empty.ends_with(L'\0'));
    evalEndsWith(empty, PathType(), true);
    evalEndsWith(empty, L"a", false);
    REQUIRE(!empty.ends_with(PathType(L"", 1)));
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(!empty.ends_with(L"\0"s));
    REQUIRE(!empty.ends_with(L"\0"sv));
#endif
}

TEST_CASE("PathViewTests::Comparison", "[path]")
{
    DoStringViewLikeComparisonTests<wil::path_view_nothrow>();
    DoStringViewLikeComparisonTests<wil::path_view_failfast>();
#ifdef WIL_ENABLE_EXCEPTIONS
    DoStringViewLikeComparisonTests<wil::path_view>();
#endif
}

template <typename PathType>
static void DoStringViewLikeSearchTests()
{
    auto findCharEval = [](PathType& path, wchar_t ch, size_t findExpect, size_t rfindExpect = PathType::npos) {
        rfindExpect = (rfindExpect == PathType::npos) ? findExpect : rfindExpect;
        auto containsExpect = findExpect != PathType::npos;
        REQUIRE(path.find(ch) == findExpect);
        REQUIRE(path.rfind(ch) == rfindExpect);
        REQUIRE(path.contains(ch) == containsExpect);
        // find_XXX_of is equivalant to (r)find with single characters
        REQUIRE(path.find_first_of(ch) == findExpect);
        REQUIRE(path.find_last_of(ch) == rfindExpect);

        // We can also treat as a string
        PathType findPath(&ch, 1);
        REQUIRE(path.find(findPath) == findExpect);
        REQUIRE(path.rfind(findPath) == rfindExpect);
        REQUIRE(path.contains(findPath) == containsExpect);
#ifdef WIL_ENABLE_EXCEPTIONS
        std::wstring str(&ch, 1);
        std::wstring_view view(&ch, 1);
        REQUIRE(path.find(str) == findExpect);
        REQUIRE(path.find(view) == findExpect);
        REQUIRE(path.rfind(str) == rfindExpect);
        REQUIRE(path.rfind(view) == rfindExpect);
        REQUIRE(path.contains(str) == containsExpect);
        REQUIRE(path.contains(view) == containsExpect);
        REQUIRE(path.find_first_of(str) == findExpect);
        REQUIRE(path.find_first_of(view) == findExpect);
        REQUIRE(path.find_last_of(str) == rfindExpect);
        REQUIRE(path.find_last_of(view) == rfindExpect);
#endif
    };

    auto findEval = [](PathType& path, PathType compare, size_t findExpect, size_t rfindExpect = PathType::npos) {
        rfindExpect = (rfindExpect != PathType::npos) ? rfindExpect : findExpect;
        auto containsExpect = findExpect != PathType::npos;
        REQUIRE(path.find(compare) == findExpect);
        REQUIRE(path.find(compare.data()) == findExpect);
        REQUIRE(path.rfind(compare) == rfindExpect);
        REQUIRE(path.rfind(compare.data()) == rfindExpect);
        REQUIRE(path.contains(compare) == containsExpect);
        REQUIRE(path.contains(compare.data()) == containsExpect);
#ifdef WIL_ENABLE_EXCEPTIONS
        std::wstring str(compare.data(), compare.size());
        std::wstring_view view(compare.data(), compare.size());
        REQUIRE(path.find(str) == findExpect);
        REQUIRE(path.find(view) == findExpect);
        REQUIRE(path.rfind(str) == rfindExpect);
        REQUIRE(path.rfind(view) == rfindExpect);
        REQUIRE(path.contains(str) == containsExpect);
        REQUIRE(path.contains(view) == containsExpect);
#endif
    };

    auto findOfEval =
        [](PathType& path, PathType compare, size_t firstOfExpect, size_t firstNotOfExpect, size_t lastOfExpect, size_t lastNotOfExpect) {
            REQUIRE(path.find_first_of(compare) == firstOfExpect);
            REQUIRE(path.find_first_of(compare.data()) == firstOfExpect);
            REQUIRE(path.find_first_not_of(compare) == firstNotOfExpect);
            REQUIRE(path.find_first_not_of(compare.data()) == firstNotOfExpect);
            REQUIRE(path.find_last_of(compare) == lastOfExpect);
            REQUIRE(path.find_last_of(compare.data()) == lastOfExpect);
            REQUIRE(path.find_last_not_of(compare) == lastNotOfExpect);
            REQUIRE(path.find_last_not_of(compare.data()) == lastNotOfExpect);
#ifdef WIL_ENABLE_EXCEPTIONS
            std::wstring str(compare.data(), compare.size());
            std::wstring_view view(compare.data(), compare.size());
            REQUIRE(path.find_first_of(str) == firstOfExpect);
            REQUIRE(path.find_first_of(view) == firstOfExpect);
            REQUIRE(path.find_first_not_of(str) == firstNotOfExpect);
            REQUIRE(path.find_first_not_of(view) == firstNotOfExpect);
            REQUIRE(path.find_last_of(str) == lastOfExpect);
            REQUIRE(path.find_last_of(view) == lastOfExpect);
            REQUIRE(path.find_last_not_of(str) == lastNotOfExpect);
            REQUIRE(path.find_last_not_of(view) == lastNotOfExpect);
#endif
        };

    PathType path(L"abcdefg");
    findCharEval(path, L'a', 0);
    findCharEval(path, L'd', 3);
    findCharEval(path, L'g', 6);
    findCharEval(path, L'\0', PathType::npos);
    findCharEval(path, L'h', PathType::npos);
    findEval(path, PathType(), 0, 7);
    findEval(path, L"abc", 0);
    findEval(path, L"abcdefg", 0);
    findEval(path, L"abcdefgh", PathType::npos);
    findEval(path, L"def", 3);
    findEval(path, L"deg", PathType::npos);
    REQUIRE(path.find(PathType(L"abcdefg", 8)) == PathType::npos);
    REQUIRE(path.rfind(PathType(L"abcdefg", 8)) == PathType::npos);
    REQUIRE(!path.contains(PathType(L"abcdefg", 8)));
    REQUIRE(path.find(PathType(L"def", 4)) == PathType::npos);
    REQUIRE(path.rfind(PathType(L"def", 4)) == PathType::npos);
    REQUIRE(!path.contains(PathType(L"def", 4)));
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(path.find(L"abcdefg\0"s) == PathType::npos);
    REQUIRE(path.find(L"abcdefg\0"sv) == PathType::npos);
    REQUIRE(path.rfind(L"abcdefg\0"s) == PathType::npos);
    REQUIRE(path.rfind(L"abcdefg\0"sv) == PathType::npos);
    REQUIRE(!path.contains(L"abcdefg\0"s));
    REQUIRE(!path.contains(L"abcdefg\0"sv));
    REQUIRE(path.find(L"def\0"s) == PathType::npos);
    REQUIRE(path.find(L"def\0"sv) == PathType::npos);
    REQUIRE(path.rfind(L"def\0"s) == PathType::npos);
    REQUIRE(path.rfind(L"def\0"sv) == PathType::npos);
    REQUIRE(!path.contains(L"def\0"s));
    REQUIRE(!path.contains(L"def\0"sv));
#endif

    findOfEval(path, L"", PathType::npos, 0, PathType::npos, 6);
    findOfEval(path, L"abc", 0, 3, 2, 6);
    findOfEval(path, L"abcdefg", 0, PathType::npos, 6, PathType::npos);
    findOfEval(path, L"gfedcba", 0, PathType::npos, 6, PathType::npos);
    findOfEval(path, L"bf", 1, 0, 5, 6);
    findOfEval(path, L"cde", 2, 0, 4, 6);
    findOfEval(path, L"👋cde🌎", 2, 0, 4, 6);
    findOfEval(path, L"xyz", PathType::npos, 0, PathType::npos, 6);
    findOfEval(path, L"👋🌎", PathType::npos, 0, PathType::npos, 6);
    REQUIRE(path.find_first_of(PathType(L"", 1)) == PathType::npos);
    REQUIRE(path.find_first_not_of(PathType(L"", 1)) == 0);
    REQUIRE(path.find_last_of(PathType(L"", 1)) == PathType::npos);
    REQUIRE(path.find_last_not_of(PathType(L"", 1)) == 6);
    REQUIRE(path.find_first_of(PathType(L"\0abc", 4)) == 0);
    REQUIRE(path.find_first_not_of(PathType(L"\0abc", 4)) == 3);
    REQUIRE(path.find_last_of(PathType(L"\0abc", 4)) == 2);
    REQUIRE(path.find_last_not_of(PathType(L"\0abc", 4)) == 6);
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(path.find_first_of(L"\0"s) == PathType::npos);
    REQUIRE(path.find_first_of(L"\0"sv) == PathType::npos);
    REQUIRE(path.find_first_not_of(L"\0"s) == 0);
    REQUIRE(path.find_first_not_of(L"\0"sv) == 0);
    REQUIRE(path.find_last_of(L"\0"s) == PathType::npos);
    REQUIRE(path.find_last_of(L"\0"sv) == PathType::npos);
    REQUIRE(path.find_last_not_of(L"\0"s) == 6);
    REQUIRE(path.find_last_not_of(L"\0"sv) == 6);
    REQUIRE(path.find_first_of(L"\0abc"s) == 0);
    REQUIRE(path.find_first_of(L"\0abc"sv) == 0);
    REQUIRE(path.find_first_not_of(L"\0abc"s) == 3);
    REQUIRE(path.find_first_not_of(L"\0abc"sv) == 3);
    REQUIRE(path.find_last_of(L"\0abc"s) == 2);
    REQUIRE(path.find_last_of(L"\0abc"sv) == 2);
    REQUIRE(path.find_last_not_of(L"\0abc"s) == 6);
    REQUIRE(path.find_last_not_of(L"\0abc"sv) == 6);
#endif

    PathType repeat(L"abcabcabc");
    findCharEval(repeat, L'a', 0, 6);
    findCharEval(repeat, L'b', 1, 7);
    findCharEval(repeat, L'c', 2, 8);
    findEval(repeat, L"abc", 0, 6);
    findEval(repeat, L"bc", 1, 7);
    REQUIRE(repeat.find(L'a', 1) == 3);
    REQUIRE(repeat.find(L'a', 3) == 3);
    REQUIRE(repeat.find(L'a', 4) == 6);
    REQUIRE(repeat.find(L'a', 7) == PathType::npos);
    REQUIRE(repeat.find(L'a', 100) == PathType::npos);
    REQUIRE(repeat.find(L"abc", 1) == 3);
    REQUIRE(repeat.find(L"abc", 3) == 3);
    REQUIRE(repeat.find(L"abc", 4) == 6);
    REQUIRE(repeat.find(L"abc", 7) == PathType::npos);
    REQUIRE(repeat.find(L"abc", 100) == PathType::npos);
    REQUIRE(repeat.rfind(L'a', 100) == 6);
    REQUIRE(repeat.rfind(L'a', 7) == 6);
    REQUIRE(repeat.rfind(L'a', 6) == 6);
    REQUIRE(repeat.rfind(L'a', 3) == 3);
    REQUIRE(repeat.rfind(L'a', 2) == 0);
    REQUIRE(repeat.rfind(L"abc", 7) == 6);
    REQUIRE(repeat.rfind(L"abc", 6) == 6);
    REQUIRE(repeat.rfind(L"abc", 3) == 3);
    REQUIRE(repeat.rfind(L"abc", 2) == 0);

    findOfEval(repeat, L"abc", 0, PathType::npos, 8, PathType::npos);
    findOfEval(repeat, L"ab", 0, 2, 7, 8);
    findOfEval(repeat, L"bc", 1, 0, 8, 6);
    findOfEval(repeat, L"aaabbbccc", 0, PathType::npos, 8, PathType::npos);

    PathType empty;
    findCharEval(empty, L'a', PathType::npos);
    findCharEval(empty, L'\0', PathType::npos);
    findEval(empty, PathType(), 0);
    REQUIRE(empty.find(PathType(), 1) == PathType::npos);
}

TEST_CASE("PathViewTests::Search", "[path]")
{
    DoStringViewLikeSearchTests<wil::path_view_nothrow>();
    DoStringViewLikeSearchTests<wil::path_view_failfast>();
#ifdef WIL_ENABLE_EXCEPTIONS
    DoStringViewLikeSearchTests<wil::path_view>();
#endif
}
