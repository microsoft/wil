#include "pch.h"

#include <memory>
#include <string>
#include <vector>
#if _HAS_CXX17
#include <optional>
#endif
#include <array>

#include <windows.h>

#include <wil/filesystem.h>
#include <wil/registry.h>
#include <wil/resource.h>

#include "common.h"

constexpr auto* testSubkey = L"Software\\Microsoft\\BasicRegistryTest";
constexpr auto* dwordValueName = L"MyDwordValue";
constexpr auto* qwordValueName = L"MyQwordValue";
constexpr auto* stringValueName = L"MyStringValue";
#if defined(WIL_ENABLE_EXCEPTIONS)
constexpr auto* multiStringValueName = L"MyMultiStringValue";
#endif
constexpr auto* invalidValueName = L"NonExistentValue";
constexpr auto* wrongTypeValueName = L"InvalidTypeValue";

constexpr uint32_t test_dword_two = 2ul;
constexpr DWORD test_dword_three = 3ul;
constexpr uint32_t test_dword_zero = 0ul;
constexpr uint64_t test_qword_zero = 0ull;
constexpr DWORD64 test_qword_max = 0xffffffffffffffff;
const std::wstring test_string_empty{};

constexpr PCWSTR test_null_terminated_string{L"testing"};
constexpr PCWSTR test_empty_null_terminated_string{L""};

constexpr PCWSTR test_enum_KeyName1 = L"1first_key";
constexpr PCWSTR test_enum_KeyName2 = L"2second_key_even_longer";
constexpr PCWSTR test_enum_KeyName3 = L"3third_key_shorter";
constexpr PCWSTR test_enum_KeyName4 = L"4fourth_key_very_very_very_very_long";

constexpr PCWSTR test_enum_valueName1 = L"1first_value";
constexpr PCWSTR test_enum_valueName2 = L"2second_value_even_longer";
constexpr PCWSTR test_enum_valueName3 = L"3third_value_shorter";
constexpr PCWSTR test_enum_valueName4 = L"4fourth_value_very_very_very_very_long";

// The empty multistring array has specific behavior: it will be read as an array with one string.
const std::vector<std::wstring> test_multistring_empty{};

constexpr PCWSTR stringLiteralArrayOfOne[1]{L""};

constexpr uint32_t test_expanded_string_buffer_size = 100;

const std::vector<DWORD> dwordTestVector = {static_cast<DWORD>(-1), 1, 0};
const std::vector<DWORD64> qwordTestVector = {static_cast<DWORD64>(-1), 1, 0};
const std::array<std::wstring, 4> stringTestArray = {L".", L"", L"Hello there!", L"\0"};
const std::wstring expandedStringTestArray[] = {L".", L"", L"%WINDIR%", L"\0"};
const std::vector<std::vector<std::wstring>> multiStringTestVector{
    std::vector<std::wstring>{{}},
    std::vector<std::wstring>{{}, {}},
    std::vector<std::wstring>{{}, {L"."}, {}, {L"."}, {}, {}},
    std::vector<std::wstring>{{L"Hello there!"}, {L"Hello a second time!"}, {L"Hello a third time!"}},
    std::vector<std::wstring>{{L""}, {L""}, {L""}},
    std::vector<std::wstring>{{L"a"}}};

const std::vector<std::vector<PCWSTR>> multiStringLiteralsTestArray{
    {L""},
    {L"", L""},
    {L"", L".", L"", L".", L"", L""},
    {L"Hello there!", L"Hello a second time!", L"Hello a third time!"},
    {L"", L"", L""},
    {L"a"}};

const std::vector<BYTE> emptyStringTestValue{};
const std::vector<BYTE> nonNullTerminatedString{'a', 0, 'b', 0, 'c', 0, 'd', 0, 'e', 0, 'f', 0,
                                                'g', 0, 'h', 0, 'i', 0, 'j', 0, 'k', 0, 'l', 0};
const std::wstring nonNullTerminatedStringFixed{L"abcdefghijkl"};

const std::vector<std::vector<BYTE>> vectorBytesTestArray{
    std::vector<BYTE>{0x00},
    std::vector<BYTE>{},
    std::vector<BYTE>{0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf}};

const std::vector<std::vector<BYTE>> multiStringRawTestVector{
    {},                                                           // empty buffer
    {0},                                                          // 1 char
    {0, 0},                                                       // 1 null terminators
    {0, 0, 0, 0},                                                 // 2 null terminators
    {0, 0, 0, 0, 0, 0},                                           // 3 null terminators
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 10 null terminators
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                            // odd number of nulls (5 1/2)
    {
        'a',
        0,
        'b',
        0,
        'c',
        0,
        'd',
        0,
    },                                                            // non-null-terminated sequence of letters
    {'a', 0, 'b', 0, 'c', 0, 'd', 0, 0},                          // odd-null-terminated sequence of letters
    {'a', 0, 'b', 0, 'c', 0, 'd', 0, 0, 0},                       // single-null-terminated sequence of letters
    {'a', 0, 'b', 0, 'c', 0, 'd', 0, 0, 0, 0},                    // odd-null-terminated sequence of letters
    {'a', 0, 'b', 0, 'c', 0, 'd', 0, 0, 0, 0, 0},                 // double-null-terminated sequence of letters
    {'a', 0, 0, 0, 'b', 0, 0, 0, 'c', 0, 0, 0, 'd', 0, 0, 0},     // null-separated sequence of letters
    {'a', 0, 'b', 0, 'c', 0, 0, 0, 'd', 0, 'e', 0, 'f', 0},       // null-separated sequence of words, no final terminator
    {'a', 0, 'b', 0, 'c', 0, 0, 0, 'd', 0, 'e', 0, 'f', 0, 0, 0}, // null-separated sequence of words, single final terminator
    {'a', 0, 'b', 0, 'c', 0, 0, 0, 'd', 0, 'e', 0, 'f', 0, 0, 0, 0, 0}, // null-separated sequence of words, double final terminator
    {'a', 0, 0, 0, 0, 0, 'b', 0, 0, 0, 0, 0, 'c', 0, 0, 0, 0, 0, 'd', 0, 0, 0, 0, 0}, // double-null-separated sequence of letters
    {'f', 0, 'o', 0, 'o', 0, 0, 0, 'b', 0, 'a', 0, 'r', 0, 0, 0},
};
const std::vector<std::vector<std::wstring>> multiStringRawExpectedValues{
    {std::wstring{}},
    {std::wstring{}},
    {std::wstring{}},
    {std::wstring{}},
    {std::wstring{}, std::wstring{}},
    {std::wstring{}, std::wstring{}, std::wstring{}, std::wstring{}, std::wstring{}, std::wstring{}, std::wstring{}, std::wstring{}, std::wstring{}},
    {std::wstring{}, std::wstring{}, std::wstring{}, std::wstring{}},
    {std::wstring{L"abcd"}},
    {std::wstring{L"abcd"}},
    {std::wstring{L"abcd"}},
    {std::wstring{L"abcd"}},
    {std::wstring{L"abcd"}},
    {std::wstring{L"a"}, std::wstring{L"b"}, std::wstring{L"c"}, std::wstring{L"d"}},
    {std::wstring{L"abc"}, std::wstring{L"def"}},
    {std::wstring{L"abc"}, std::wstring{L"def"}},
    {std::wstring{L"abc"}, std::wstring{L"def"}},
    {std::wstring{L"a"}, std::wstring{}, std::wstring{L"b"}, std::wstring{}, std::wstring{L"c"}, std::wstring{}, std::wstring{L"d"}},
    {std::wstring{L"foo"}, std::wstring{L"bar"}},
};

const wchar_t* enumTestNames[] = {
    L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    L"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    L"cccccccccccccccccccccc",
    L"dddddddddddddddddddddddddddddddd",
    L"eeeee",
    L""};

wil::unique_cotaskmem_array_ptr<BYTE> cotaskmemArrayBytesTestArray[3];
void PopulateCoTaskMemArrayTestCases()
{
    cotaskmemArrayBytesTestArray[0].reset(static_cast<BYTE*>(CoTaskMemAlloc(1)), 1);
    cotaskmemArrayBytesTestArray[0][0] = 0x00;

    cotaskmemArrayBytesTestArray[1].reset();

    cotaskmemArrayBytesTestArray[2].reset(static_cast<BYTE*>(CoTaskMemAlloc(15)), 15);
    constexpr BYTE thirdTestcaseData[]{0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    CopyMemory(cotaskmemArrayBytesTestArray[2].get(), thirdTestcaseData, 15);
}

bool AreStringsEqual(const std::wstring& lhs, const std::wstring& rhs) noexcept
{
    return lhs == rhs;
}

bool AreStringsEqual(const wil::unique_bstr& lhs, const std::wstring& rhs) noexcept
{
    if (!lhs && rhs.empty())
    {
        return true;
    }
    if (SysStringLen(lhs.get()) != rhs.length())
    {
        printf("String lengths don't match: BSTR (%ws) %u, wstring (%ws) %zu\n", lhs.get(), SysStringLen(lhs.get()), rhs.c_str(), rhs.length());
        return false;
    }
    return (0 == wcscmp(lhs.get(), rhs.c_str()));
}

#if defined(__WIL_OLEAUTO_H_STL)
bool AreStringsEqual(const wil::shared_bstr& lhs, const std::wstring& rhs) noexcept
{
    if (!lhs && rhs.empty())
    {
        return true;
    }
    if (SysStringLen(lhs.get()) != rhs.length())
    {
        printf("String lengths don't match: BSTR (%ws) %u, wstring (%ws) %zu\n", lhs.get(), SysStringLen(lhs.get()), rhs.c_str(), rhs.length());
        return false;
    }
    return (0 == wcscmp(lhs.get(), rhs.c_str()));
}
#endif

bool AreStringsEqual(const wil::unique_cotaskmem_string& lhs, const std::wstring& rhs) noexcept
{
    if (!lhs && rhs.empty())
    {
        return true;
    }
    return (0 == wcscmp(lhs.get(), rhs.c_str()));
}

#if defined(__WIL_OBJBASE_H_STL)
bool AreStringsEqual(const wil::shared_cotaskmem_string& lhs, const std::wstring& rhs) noexcept
{
    if (!lhs && rhs.empty())
    {
        return true;
    }
    return (0 == wcscmp(lhs.get(), rhs.c_str()));
}
#endif

template <size_t C>
bool AreStringsEqual(wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string>& cotaskmemarray_strings, const PCWSTR array_literals[C])
{
    if (C != cotaskmemarray_strings.size())
    {
        wprintf(L"array_literals[C] size (%zu) is not equal to cotaskmemarray_strings.size() (%zu)", C, cotaskmemarray_strings.size());
        return false;
    }

    for (size_t i = 0; i < C; ++i)
    {
        if (wcscmp(cotaskmemarray_strings[i], array_literals[i]) != 0)
        {
            wprintf(L"array_literals[i] (%ws) is not equal to cotaskmemarray_strings[i] (%ws)", array_literals[i], cotaskmemarray_strings[i]);
            return false;
        }
    }

    return true;
}

bool AreStringsEqual(wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string>& cotaskmem_array, const std::vector<std::wstring>& wstring_vector)
{
    if (cotaskmem_array.size() != wstring_vector.size())
    {
        printf(
            "container lengths don't match: unique_cotaskmem_array_ptr %zu, vector %zu\n", cotaskmem_array.size(), wstring_vector.size());
        return false;
    }

    for (size_t i = 0; i < cotaskmem_array.size(); ++i)
    {
        const auto& cotaskmem_string = cotaskmem_array[i];
        const auto cotaskmem_string_length = wcslen(cotaskmem_string);
        const auto& wstring_value = wstring_vector[i];

        if (cotaskmem_string_length != wstring_value.size())
        {
            printf(
                "string lengths don't match: unique_cotaskmem_string (%ws) %zu, wstring (%ws) %zu\n",
                cotaskmem_string,
                cotaskmem_string_length,
                wstring_value.c_str(),
                wstring_value.size());
            return false;
        }

        if (wstring_value.empty())
        {
            if (cotaskmem_string_length != 0)
            {
                printf(
                    "string don't match: unique_cotaskmem_string (%ws) %zu, wstring (%ws) %zu\n",
                    cotaskmem_string,
                    cotaskmem_string_length,
                    wstring_value.c_str(),
                    wstring_value.size());
                return false;
            }
        }
        else
        {
            if (wcscmp(cotaskmem_string, wstring_value.c_str()) != 0)
            {
                printf(
                    "string don't match: unique_cotaskmem_string (%ws) %zu, wstring (%ws) %zu\n",
                    cotaskmem_string,
                    cotaskmem_string_length,
                    wstring_value.c_str(),
                    wstring_value.size());
                return false;
            }
        }
    }

    return true;
}

bool AreStringsEqual(const wil::unique_cotaskmem_array_ptr<BYTE>& lhs, const std::vector<BYTE>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        wprintf(L"lhs size (%zu) is not equal to rhs.size() (%zu)", lhs.size(), rhs.size());
        return false;
    }

    for (size_t i = 0; i < lhs.size(); ++i)
    {
        if (lhs[i] != rhs[i])
        {
            wprintf(L"The value in lhs[i] (%c) is not equal to rhs[i] (%c)", lhs[i], rhs[i]);
            return false;
        }
    }

    return true;
}

#if defined WIL_ENABLE_EXCEPTIONS
void VerifyThrowsHr(HRESULT hr, const std::function<void()>& fn)
{
    try
    {
        fn();
        // Should not hit this
        REQUIRE(false);
    }
    catch (const wil::ResultException& e)
    {
        REQUIRE(e.GetErrorCode() == hr);
    }
}
#endif

// NOTE: these tests contain the code used in the documentation.
//
// They don't assert much: they simply validate that the code in the
// documentation works.
#if defined(WIL_ENABLE_EXCEPTIONS)
TEST_CASE("BasicRegistryTests::ExampleUsage", "[registry]")
{
    // These examples use the explicit registry key, to make the usage more
    // obvious. Just assert that these are the same thing.
    REQUIRE(std::wstring(L"Software\\Microsoft\\BasicRegistryTest") == testSubkey);

    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    // Disable "unused variable" warnings for these examples
#pragma warning(disable : 4189)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#endif
    SECTION("Basic read/write")
    {
        const DWORD showTypeOverlay = wil::reg::get_value_dword(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"ShowTypeOverlay");
        // Disabled since it writes real values.
        // wil::reg::set_value_dword(
        //    HKEY_CURRENT_USER,
        //    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        //    L"ShowTypeOverlay",
        //    1);
    }

    SECTION("Open & create keys")
    {
        // "Open" guaranteed-existing keys or "create" to potentially create if non-existent
        const auto r_unique_key =
            wil::reg::open_unique_key(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer");
        const auto rw_shared_key = wil::reg::create_shared_key(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", wil::reg::key_access::readwrite);

        // nothrow version, if you don't have exceptions
        wil::unique_hkey nothrow_key;
        THROW_IF_FAILED(wil::reg::open_unique_key_nothrow(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer", nothrow_key, wil::reg::key_access::readwrite));
    }

#if defined(__cpp_lib_optional)
    SECTION("Read values")
    {
        // Get values (or try_get if the value might not exist)
        const DWORD dword = wil::reg::get_value_dword(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme");
        const std::optional<std::wstring> stringOptional =
            wil::reg::try_get_value_string(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes", L"CurrentTheme");

        // Known HKEY
        const auto key =
            wil::reg::open_unique_key(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
        const DWORD otherDword = wil::reg::get_value_dword(key.get(), L"AppsUseLightTheme");

        // nothrow version, if you don't have exceptions
        wil::unique_bstr bstr;
        THROW_IF_FAILED(wil::reg::get_value_string_nothrow(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes", L"CurrentTheme", bstr));

        // Templated version
        const auto value = wil::reg::get_value<std::wstring>(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes", L"CurrentTheme");
    }
#endif // defined(__cpp_lib_optional)

    SECTION("Write values")
    {
        // Set values
        wil::reg::set_value_dword(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"DwordValue", 18);
        wil::reg::set_value_string(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"StringValue", L"Wowee zowee");

        // Generic versions, if you don't want to specify type.
        wil::reg::set_value(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"DwordValue2", 1);
        wil::reg::set_value(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"StringValue2", L"Besto wuz here");

        // Known HKEY
        const auto key =
            wil::reg::create_unique_key(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", wil::reg::key_access::readwrite);
        wil::reg::set_value_dword(key.get(), L"DwordValue3", 42);

        // nothrow version, if you don't have exceptions
        THROW_IF_FAILED(wil::reg::set_value_string_nothrow(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"StringValue3", L"Hi, Mom!"));

        // --- validation, not included in documentation ---

        REQUIRE(wil::reg::get_value_dword(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"DwordValue") == 18);
        REQUIRE(wil::reg::get_value_string(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"StringValue") == L"Wowee zowee");
        REQUIRE(wil::reg::get_value_dword(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"DwordValue2") == 1);
        REQUIRE(wil::reg::get_value_string(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"StringValue2") == L"Besto wuz here");
        REQUIRE(wil::reg::get_value_dword(key.get(), L"DwordValue3") == 42);
        REQUIRE(wil::reg::get_value_string(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", L"StringValue3") == L"Hi, Mom!");
    }

    SECTION("Helper functions")
    {
        const auto key =
            wil::reg::create_unique_key(HKEY_CURRENT_USER, L"Software\\Microsoft\\BasicRegistryTest", wil::reg::key_access::readwrite);

        // Get count of child keys and values.
        const uint32_t childValCount = wil::reg::get_child_value_count(key.get());
        const uint32_t childKeyCount = wil::reg::get_child_key_count(key.get());
        const uint32_t largeChildKeyCount = wil::reg::get_child_key_count(HKEY_CLASSES_ROOT);

        // Get last write time
        const FILETIME lastModified = wil::reg::get_last_write_filetime(key.get());

        // Simple helpers for analyzing returned HRESULTs
        const bool a = wil::reg::is_registry_buffer_too_small(HRESULT_FROM_WIN32(ERROR_MORE_DATA)); // => true
        const bool b = wil::reg::is_registry_not_found(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));   // => true
        const bool c = wil::reg::is_registry_not_found(HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));   // => true

        // --- validation, not included in documentation ---
        REQUIRE(childKeyCount == 0);
        REQUIRE(childValCount == 0);
        REQUIRE(largeChildKeyCount > 1000);
        REQUIRE(a == true);
        REQUIRE(b == true);
        REQUIRE(c == true);
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#pragma warning(default : 4189)
}
#endif // defined(WIL_ENABLE_EXCEPTIONS)

TEST_CASE("BasicRegistryTests::Open", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("open_unique_key_nothrow: with opened key")
    {
        constexpr auto* subSubKey = L"subkey";

        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        // create a sub-key under this which we will try to open - but open_key will use the above hkey
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(hkey.get(), subSubKey, subkey, wil::reg::key_access::readwrite));
        // write a test value we'll try to read from later
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(subkey.get(), dwordValueName, test_dword_two));
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(subkey.get(), qwordValueName, test_qword_max));

        wil::unique_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);
        DWORD64 result_dword64{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(opened_key.get(), qwordValueName, &result_dword64));
        REQUIRE(result_dword64 == test_qword_max);

        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three));
        unsigned int result_int{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result_int));
        REQUIRE(result_int == test_dword_three);
        uint64_t result_uint64{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(opened_key.get(), qwordValueName, &result_uint64));
        REQUIRE(result_uint64 == test_qword_max);

        // fail open if the key doesn't exist
        hr = wil::reg::open_unique_key_nothrow(
            hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));

        hr = wil::reg::open_unique_key_nothrow(
            hkey.get(), (std::wstring(subSubKey) + L"\\not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));
    }
    SECTION("open_unique_key_nothrow: with string key")
    {
        // create read-write, should be able to open read and open read-write
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        // write a test value
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, test_dword_two));
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(hkey.get(), qwordValueName, test_qword_max));

        wil::unique_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);
        DWORD64 result_dword64{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(opened_key.get(), qwordValueName, &result_dword64));
        REQUIRE(result_dword64 == test_qword_max);

        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three));
        unsigned int result_int{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result_int));
        REQUIRE(result_int == test_dword_three);
        uint64_t result_uint64{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(opened_key.get(), qwordValueName, &result_uint64));
        REQUIRE(result_uint64 == test_qword_max);

        // fail open if the key doesn't exist
        hr = wil::reg::open_unique_key_nothrow(
            HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));

        hr = wil::reg::open_unique_key_nothrow(
            HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"\\not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));
    }
    SECTION("get_child_key_count_nothrow, get_child_value_count_nothrow")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        DWORD keyCount{};
        REQUIRE_SUCCEEDED(wil::reg::get_child_key_count_nothrow(hkey.get(), &keyCount));
        REQUIRE(keyCount == 0);

        DWORD valueCount{};
        REQUIRE_SUCCEEDED(wil::reg::get_child_value_count_nothrow(hkey.get(), &valueCount));
        REQUIRE(valueCount == 0);

        wil::unique_hkey testKey; // will just reuse the same RAII object

        const auto testkey1 = std::wstring(testSubkey) + L"\\1";
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testkey1.c_str(), testKey, wil::reg::key_access::read));
        const auto testkey2 = std::wstring(testSubkey) + L"\\2";
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testkey2.c_str(), testKey, wil::reg::key_access::read));
        const auto testkey3 = std::wstring(testSubkey) + L"\\3";
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testkey3.c_str(), testKey, wil::reg::key_access::read));
        const auto testkey4 = std::wstring(testSubkey) + L"\\4\\4";
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testkey4.c_str(), testKey, wil::reg::key_access::read));
        const auto testkey5 = std::wstring(testSubkey) + L"\\5\\5\\5";
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testkey5.c_str(), testKey, wil::reg::key_access::read));

        hkey.reset();
        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, 1ul));
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(hkey.get(), qwordValueName, 2ull));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, L"three"));
        REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(
            hkey.get(), (std::wstring(stringValueName) + L"_expanded").c_str(), L"%four%"));

        REQUIRE_SUCCEEDED(wil::reg::get_child_key_count_nothrow(hkey.get(), &keyCount));
        REQUIRE(keyCount == 5);

        REQUIRE_SUCCEEDED(wil::reg::get_child_value_count_nothrow(hkey.get(), &valueCount));
        REQUIRE(valueCount == 4);
    }
#if defined(__WIL_WINREG_STL)
    SECTION("open_shared_key_nothrow: with opened key")
    {
        constexpr auto* subSubKey = L"subkey";

        wil::shared_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_shared_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        // create a sub-key under this which we will try to open - but open_key will use the above hkey
        wil::shared_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_shared_key_nothrow(hkey.get(), subSubKey, subkey, wil::reg::key_access::readwrite));
        // write a test value we'll try to read from later
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(subkey.get(), dwordValueName, test_dword_two));
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(subkey.get(), qwordValueName, test_qword_max));

        wil::shared_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);
        DWORD64 result_dword64{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(opened_key.get(), qwordValueName, &result_dword64));
        REQUIRE(result_dword64 == test_qword_max);

        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three));
        uint32_t result_int{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result_int));
        REQUIRE(result_int == test_dword_three);
        uint64_t result_uint64{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(opened_key.get(), qwordValueName, &result_uint64));
        REQUIRE(result_uint64 == test_qword_max);

        // fail open if the key doesn't exist
        hr = wil::reg::open_shared_key_nothrow(
            hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));
    }
    SECTION("open_shared_key_nothrow: with string key")
    {
        // create read-write, should be able to open read and open read-write
        wil::shared_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_shared_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        // write a test value
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, test_dword_two));
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(hkey.get(), qwordValueName, test_qword_max));

        wil::shared_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);
        DWORD64 result_dword64{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(opened_key.get(), qwordValueName, &result_dword64));
        REQUIRE(result_dword64 == test_qword_max);

        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three));
        uint32_t result_int{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result_int));
        REQUIRE(result_int == test_dword_three);
        uint64_t result_uint64{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(opened_key.get(), qwordValueName, &result_uint64));
        REQUIRE(result_uint64 == test_qword_max);

        // fail open if the key doesn't exist
        hr = wil::reg::open_shared_key_nothrow(
            HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));
    }
#endif // #if defined(__WIL_WINREG_STL)

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("open_unique_key: with opened key")
    {
        constexpr auto* subSubKey = L"subkey";

        const wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        // create a sub-key under this which we will try to open - but open_key will use the above hkey
        const wil::unique_hkey subkey{wil::reg::create_unique_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite)};
        // write a test value we'll try to read from later
        wil::reg::set_value_dword(subkey.get(), dwordValueName, test_dword_two);

        const wil::unique_hkey read_only_key{wil::reg::open_unique_key(hkey.get(), subSubKey, wil::reg::key_access::read)};
        DWORD result = wil::reg::get_value_dword(read_only_key.get(), dwordValueName);
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        const wil::unique_hkey read_write_key{wil::reg::open_unique_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite)};
        wil::reg::set_value_dword(read_write_key.get(), dwordValueName, test_dword_three);
        result = wil::reg::get_value_dword(read_write_key.get(), dwordValueName);
        REQUIRE(result == test_dword_three);

        // fail get* if the value doesn't exist
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]() {
            const wil::unique_hkey invalid_key{wil::reg::open_unique_key(
                hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite)};
        });
    }

    SECTION("open_unique_key: with string key")
    {
        const wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        // write a test value we'll try to read from later
        wil::reg::set_value_dword(hkey.get(), dwordValueName, test_dword_two);

        const wil::unique_hkey read_only_key{wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::read)};
        DWORD result = wil::reg::get_value_dword(read_only_key.get(), dwordValueName);
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        const wil::unique_hkey read_write_key{wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value_dword(read_write_key.get(), dwordValueName, test_dword_three);
        result = wil::reg::get_value_dword(read_write_key.get(), dwordValueName);
        REQUIRE(result == test_dword_three);

        // fail get* if the value doesn't exist
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]() {
            const wil::unique_hkey invalid_key{wil::reg::open_unique_key(
                HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite)};
        });
    }

    SECTION("get_child_key_count, get_child_value_count")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        auto keyCount = wil::reg::get_child_key_count(hkey.get());
        REQUIRE(keyCount == 0);

        auto valueCount = wil::reg::get_child_value_count(hkey.get());
        REQUIRE(valueCount == 0);

        wil::unique_hkey testKey; // will just reuse the same RAII object

        const auto testkey1 = std::wstring(testSubkey) + L"\\1";
        testKey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testkey1.c_str());
        const auto testkey2 = std::wstring(testSubkey) + L"\\2";
        testKey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testkey2.c_str());
        const auto testkey3 = std::wstring(testSubkey) + L"\\3";
        testKey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testkey3.c_str());
        const auto testkey4 = std::wstring(testSubkey) + L"\\4\\4";
        testKey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testkey4.c_str());
        const auto testkey5 = std::wstring(testSubkey) + L"\\5\\5\\5";
        testKey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testkey5.c_str());

        hkey = wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);

        wil::reg::set_value_dword(hkey.get(), dwordValueName, 1ul);
        wil::reg::set_value_qword(hkey.get(), qwordValueName, 2ull);
        wil::reg::set_value_string(hkey.get(), stringValueName, L"three");
        wil::reg::set_value_expanded_string(hkey.get(), (std::wstring(stringValueName) + L"_expanded").c_str(), L"%four%");

        keyCount = wil::reg::get_child_key_count(hkey.get());
        REQUIRE(keyCount == 5);

        valueCount = wil::reg::get_child_value_count(hkey.get());
        REQUIRE(valueCount == 4);
    }

#if defined(__WIL_WINREG_STL)
    SECTION("open_shared_key: with opened key")
    {
        constexpr auto* subSubKey = L"subkey";

        const wil::shared_hkey hkey{wil::reg::create_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        // create a sub-key under this which we will try to open - but open_key will use the above hkey
        const wil::shared_hkey subkey{wil::reg::create_shared_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite)};
        // write a test value we'll try to read from later
        wil::reg::set_value_dword(subkey.get(), dwordValueName, test_dword_two);

        const wil::shared_hkey read_only_key{wil::reg::open_shared_key(hkey.get(), subSubKey, wil::reg::key_access::read)};
        DWORD result = wil::reg::get_value_dword(read_only_key.get(), dwordValueName);
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        const wil::shared_hkey read_write_key{wil::reg::open_shared_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite)};
        wil::reg::set_value_dword(read_write_key.get(), dwordValueName, test_dword_three);
        result = wil::reg::get_value_dword(read_write_key.get(), dwordValueName);
        REQUIRE(result == test_dword_three);

        // fail get* if the value doesn't exist
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]() {
            const wil::shared_hkey invalid_key{wil::reg::open_shared_key(
                hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite)};
        });
    }

    SECTION("open_shared_key: with string key")
    {
        const wil::shared_hkey hkey{wil::reg::create_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        // write a test value we'll try to read from later
        wil::reg::set_value_dword(hkey.get(), dwordValueName, test_dword_two);

        const wil::shared_hkey read_only_key{wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::read)};
        DWORD result = wil::reg::get_value_dword(read_only_key.get(), dwordValueName);
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        const wil::shared_hkey read_write_key{wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value_dword(read_write_key.get(), dwordValueName, test_dword_three);
        result = wil::reg::get_value_dword(read_write_key.get(), dwordValueName);
        REQUIRE(result == test_dword_three);

        // fail get* if the value doesn't exist
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]() {
            const wil::shared_hkey invalid_key{wil::reg::open_shared_key(
                HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite)};
        });
    }
#endif
#endif
}

namespace
{
// Mimic C++20 type_identity to avoid trying to template-deduce in
// function pointers.
//
// https://en.cppreference.com/w/cpp/types/type_identity
template <typename T>
struct type_identity
{
    using type = T;
};
} // namespace

namespace
{
// This test matrix is *huge*! We have:
//
// - ~6 registry types (DWORDs, QWORDs, strings, expanded strings,
//   multistrings, and binary data) *and* many have different
//   representations (like strings and expanded strings, which can each be
//   read into multiple concrete string types).
// - 3 ways to fetch (get, try_get, nothrow)
// - 2 calling patterns (generic get_value & typed get_value_*)
// - 2 key access methods (opened HKEYs and string subkeys)
//
// This section tests simple types, like DWORDs, QWORDs, and (oddly)
// multistrings, plus generic versions (eg get_value<DWORD>) where
// applicable, across get, try_get, and nothrow for both string keys and
// opened keys. We test strings, expanded strings, and binary data later.
// (We test multistrings here because we currently only support reading into
// a std::vector<std::wstring>, which fits nicely into this test format).
//
// (DWORD, generic DWORD, QWORD, generic QWORD, multistring)
//
// x
//
// (nothrow opened key, nothrow string key, get opened key, get string key,
// try_get opened key, try_get string key)
//
// To create that test matrix, these tests use basic structs, with a
// consistent set of static methods, that are passed into each test. This
// should be fairly easy to generalize to other types if we need to add any
// later.
//
// However, strings (including expanded strings) and binary data require
// slightly different tests. We separated those tests out for clarity.
//
// We also have separate tests for edge cases (for example, reading strings
// without nullptr terminators, or reading completely blank multistrings).

template <typename RetType, typename SetType>
struct GenericBaseFns
{
    static HRESULT set_nothrow(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value)
    {
        return wil::reg::set_value_nothrow(key.get(), valueName, value);
    }
    static HRESULT set_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value)
    {
        return wil::reg::set_value_nothrow(key, subkey, valueName, value);
    }

    static HRESULT get_nothrow(wil::unique_hkey const& key, PCWSTR valueName, RetType* output)
    {
        return wil::reg::get_value_nothrow(key.get(), valueName, output);
    }
    static HRESULT get_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, RetType* output)
    {
        return wil::reg::get_value_nothrow(key, subkey, valueName, output);
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    static void set(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value)
    {
        wil::reg::set_value(key.get(), valueName, value);
    }
    static void set(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value)
    {
        wil::reg::set_value(key, subkey, valueName, value);
    }

    static RetType get(wil::unique_hkey const& key, PCWSTR valueName)
    {
        return wil::reg::get_value<RetType>(key.get(), valueName);
    }
    static RetType get(HKEY key, PCWSTR subkey, PCWSTR valueName)
    {
        return wil::reg::get_value<RetType>(key, subkey, valueName);
    }
#if defined(__cpp_lib_optional)
    static std::optional<RetType> try_get(wil::unique_hkey const& key, PCWSTR valueName)
    {
        return wil::reg::try_get_value<RetType>(key.get(), valueName);
    }
    static std::optional<RetType> try_get(HKEY key, PCWSTR subkey, PCWSTR valueName)
    {
        return wil::reg::try_get_value<RetType>(key, subkey, valueName);
    }
#endif // defined(__cpp_lib_optional)
#endif // defined(WIL_ENABLE_EXCEPTIONS)
};

struct DwordFns
{
    using RetType = DWORD;
    using SetType = uint32_t;

    static std::vector<RetType> testValues()
    {
        return dwordTestVector;
    }
    static PCWSTR testValueName()
    {
        return dwordValueName;
    }

    static std::vector<std::function<HRESULT(wil::unique_hkey const&, PCWSTR)>> set_wrong_value_fns_openkey()
    {
        return {[](wil::unique_hkey const& key, PCWSTR value_name) {
            return wil::reg::set_value_qword_nothrow(key.get(), value_name, test_qword_zero);
        }};
    }

    static std::vector<std::function<HRESULT(HKEY, PCWSTR, PCWSTR)>> set_wrong_value_fns_subkey()
    {
        return {[](HKEY key, PCWSTR subkey, PCWSTR value_name) {
            return wil::reg::set_value_qword_nothrow(key, subkey, value_name, test_qword_zero);
        }};
    }

    static HRESULT set_nothrow(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value)
    {
        return wil::reg::set_value_dword_nothrow(key.get(), valueName, value);
    }
    static HRESULT set_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value)
    {
        return wil::reg::set_value_dword_nothrow(key, subkey, valueName, value);
    }

    static HRESULT get_nothrow(wil::unique_hkey const& key, PCWSTR valueName, RetType* output)
    {
        return wil::reg::get_value_dword_nothrow(key.get(), valueName, output);
    }
    static HRESULT get_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, RetType* output)
    {
        return wil::reg::get_value_dword_nothrow(key, subkey, valueName, output);
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    static void set(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value)
    {
        wil::reg::set_value_dword(key.get(), valueName, value);
    }
    static void set(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value)
    {
        wil::reg::set_value_dword(key, subkey, valueName, value);
    }

    static RetType get(wil::unique_hkey const& key, PCWSTR valueName)
    {
        return wil::reg::get_value_dword(key.get(), valueName);
    }
    static RetType get(HKEY key, PCWSTR subkey, PCWSTR valueName)
    {
        return wil::reg::get_value_dword(key, subkey, valueName);
    }
#if defined(__cpp_lib_optional)
    static std::optional<RetType> try_get(wil::unique_hkey const& key, PCWSTR valueName)
    {
        return wil::reg::try_get_value_dword(key.get(), valueName);
    }
    static std::optional<RetType> try_get(HKEY key, PCWSTR subkey, PCWSTR valueName)
    {
        return wil::reg::try_get_value_dword(key, subkey, valueName);
    }
#endif // defined(__cpp_lib_optional)
#endif // defined(WIL_ENABLE_EXCEPTIONS)
};

struct GenericDwordFns : GenericBaseFns<DWORD, uint32_t>
{
    using RetType = DWORD;
    using SetType = uint32_t;

    static std::vector<RetType> testValues()
    {
        return dwordTestVector;
    }
    static PCWSTR testValueName()
    {
        return dwordValueName;
    }

    static std::vector<std::function<HRESULT(wil::unique_hkey const&, PCWSTR)>> set_wrong_value_fns_openkey()
    {
        return {[](wil::unique_hkey const& key, PCWSTR value_name) {
            return wil::reg::set_value_qword_nothrow(key.get(), value_name, test_qword_zero);
        }};
    }

    static std::vector<std::function<HRESULT(HKEY, PCWSTR, PCWSTR)>> set_wrong_value_fns_subkey()
    {
        return {[](HKEY key, PCWSTR subkey, PCWSTR value_name) {
            return wil::reg::set_value_qword_nothrow(key, subkey, value_name, test_qword_zero);
        }};
    }
};

struct QwordFns
{
    using RetType = DWORD64;
    using SetType = uint64_t;

    static std::vector<RetType> testValues()
    {
        return qwordTestVector;
    }
    static PCWSTR testValueName()
    {
        return qwordValueName;
    }

    static std::vector<std::function<HRESULT(wil::unique_hkey const&, PCWSTR)>> set_wrong_value_fns_openkey()
    {
        return {[](wil::unique_hkey const& key, PCWSTR value_name) {
            return wil::reg::set_value_dword_nothrow(key.get(), value_name, test_dword_zero);
        }};
    }

    static std::vector<std::function<HRESULT(HKEY, PCWSTR, PCWSTR)>> set_wrong_value_fns_subkey()
    {
        return {[](HKEY key, PCWSTR subkey, PCWSTR value_name) {
            return wil::reg::set_value_dword_nothrow(key, subkey, value_name, test_dword_zero);
        }};
    }

    static HRESULT set_nothrow(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value)
    {
        return wil::reg::set_value_qword_nothrow(key.get(), valueName, value);
    }
    static HRESULT set_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value)
    {
        return wil::reg::set_value_qword_nothrow(key, subkey, valueName, value);
    }

    static HRESULT get_nothrow(wil::unique_hkey const& key, PCWSTR valueName, RetType* output)
    {
        return wil::reg::get_value_qword_nothrow(key.get(), valueName, output);
    }
    static HRESULT get_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, RetType* output)
    {
        return wil::reg::get_value_qword_nothrow(key, subkey, valueName, output);
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    static void set(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value)
    {
        wil::reg::set_value_qword(key.get(), valueName, value);
    }
    static void set(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value)
    {
        wil::reg::set_value_qword(key, subkey, valueName, value);
    }

    static RetType get(wil::unique_hkey const& key, PCWSTR valueName)
    {
        return wil::reg::get_value_qword(key.get(), valueName);
    }
    static RetType get(HKEY key, PCWSTR subkey, PCWSTR valueName)
    {
        return wil::reg::get_value_qword(key, subkey, valueName);
    }

#if defined(__cpp_lib_optional)
    static std::optional<RetType> try_get(wil::unique_hkey const& key, PCWSTR valueName)
    {
        return wil::reg::try_get_value_qword(key.get(), valueName);
    }
    static std::optional<RetType> try_get(HKEY key, PCWSTR subkey, PCWSTR valueName)
    {
        return wil::reg::try_get_value_qword(key, subkey, valueName);
    }
#endif // defined(__cpp_lib_optional)
#endif // defined(WIL_ENABLE_EXCEPTIONS)
};

struct GenericQwordFns : GenericBaseFns<DWORD64, uint64_t>
{
    using RetType = DWORD64;
    using SetType = uint64_t;

    static std::vector<RetType> testValues()
    {
        return qwordTestVector;
    }
    static PCWSTR testValueName()
    {
        return qwordValueName;
    }

    static std::vector<std::function<HRESULT(wil::unique_hkey const&, PCWSTR)>> set_wrong_value_fns_openkey()
    {
        return {[](wil::unique_hkey const& key, PCWSTR value_name) {
            return wil::reg::set_value_dword_nothrow(key.get(), value_name, test_dword_zero);
        }};
    }

    static std::vector<std::function<HRESULT(HKEY, PCWSTR, PCWSTR)>> set_wrong_value_fns_subkey()
    {
        return {[](HKEY key, PCWSTR subkey, PCWSTR value_name) {
            return wil::reg::set_value_dword_nothrow(key, subkey, value_name, test_dword_zero);
        }};
    }
};

#if defined(WIL_ENABLE_EXCEPTIONS)
struct MultiStringVectorFns
{
    using RetType = std::vector<std::wstring>;
    using SetType = std::vector<std::wstring>;

    static std::vector<RetType> testValues()
    {
        return multiStringTestVector;
    }
    static PCWSTR testValueName()
    {
        return multiStringValueName;
    }

    static std::vector<std::function<HRESULT(wil::unique_hkey const&, PCWSTR)>> set_wrong_value_fns_openkey()
    {
        return {
            [](wil::unique_hkey const& key, PCWSTR value_name) {
                return wil::reg::set_value_dword_nothrow(key.get(), value_name, test_dword_zero);
            },
            [](wil::unique_hkey const& key, PCWSTR value_name) {
                return wil::reg::set_value_string_nothrow(key.get(), value_name, test_string_empty.c_str());
            },
        };
    }

    static std::vector<std::function<HRESULT(HKEY, PCWSTR, PCWSTR)>> set_wrong_value_fns_subkey()
    {
        return {
            [](HKEY key, PCWSTR subkey, PCWSTR value_name) {
                return wil::reg::set_value_dword_nothrow(key, subkey, value_name, test_dword_zero);
            },
            [](HKEY key, PCWSTR subkey, PCWSTR value_name) {
                return wil::reg::set_value_string_nothrow(key, subkey, value_name, test_string_empty.c_str());
            },
        };
    }

    static void set(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value)
    {
        wil::reg::set_value_multistring(key.get(), valueName, value);
    }
    static void set(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value)
    {
        wil::reg::set_value_multistring(key, subkey, valueName, value);
    }

    static RetType get(wil::unique_hkey const& key, PCWSTR valueName)
    {
        return wil::reg::get_value_multistring(key.get(), valueName);
    }
    static RetType get(HKEY key, PCWSTR subkey, PCWSTR valueName)
    {
        return wil::reg::get_value_multistring(key, subkey, valueName);
    }

#if defined(__cpp_lib_optional)
    static std::optional<RetType> try_get(wil::unique_hkey const& key, PCWSTR valueName)
    {
        return wil::reg::try_get_value_multistring(key.get(), valueName);
    }
    static std::optional<RetType> try_get(HKEY key, PCWSTR subkey, PCWSTR valueName)
    {
        return wil::reg::try_get_value_multistring(key, subkey, valueName);
    }
#endif // defined(__cpp_lib_optional)
};

struct GenericMultiStringVectorFns : GenericBaseFns<std::vector<std::wstring>, std::vector<std::wstring>>
{
    using RetType = std::vector<std::wstring>;
    using SetType = std::vector<std::wstring>;

    static std::vector<RetType> testValues()
    {
        return multiStringTestVector;
    }
    static PCWSTR testValueName()
    {
        return multiStringValueName;
    }

    static std::vector<std::function<HRESULT(wil::unique_hkey const&, PCWSTR)>> set_wrong_value_fns_openkey()
    {
        return {[](wil::unique_hkey const& key, PCWSTR value_name) {
            return wil::reg::set_value_dword_nothrow(key.get(), value_name, test_dword_zero);
        }};
    }

    static std::vector<std::function<HRESULT(HKEY, PCWSTR, PCWSTR)>> set_wrong_value_fns_subkey()
    {
        return {[](HKEY key, PCWSTR subkey, PCWSTR value_name) {
            return wil::reg::set_value_dword_nothrow(key, subkey, value_name, test_dword_zero);
        }};
    }
};
#endif // defined(WIL_ENABLE_EXCEPTIONS)

#if defined(WIL_ENABLE_EXCEPTIONS)
using NoThrowTypesToTest = std::tuple<DwordFns, GenericDwordFns, QwordFns, GenericQwordFns>;
using ThrowingTypesToTest =
    std::tuple<DwordFns, GenericDwordFns, QwordFns, GenericQwordFns, MultiStringVectorFns, GenericMultiStringVectorFns>;
#else
using NoThrowTypesToTest = std::tuple<DwordFns, GenericDwordFns, QwordFns, GenericQwordFns>;
using ThrowingTypesToTest = std::tuple<DwordFns, GenericDwordFns, QwordFns, GenericQwordFns>;
#endif // defined(WIL_ENABLE_EXCEPTIONS)
} // namespace

TEMPLATE_LIST_TEST_CASE("BasicRegistryTests::simple types typed nothrow gets/sets", "[registry]", NoThrowTypesToTest)
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("get_nothrow")
    {
        SECTION("with opened key")
        {
            wil::unique_hkey hkey;
            REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

            for (auto&& value : TestType::testValues())
            {
                typename TestType::RetType result{};
                REQUIRE_SUCCEEDED(TestType::set_nothrow(hkey, TestType::testValueName(), value));
                REQUIRE_SUCCEEDED(TestType::get_nothrow(hkey, TestType::testValueName(), &result));
                REQUIRE(result == value);

                // verify reusing the previously allocated buffer
                REQUIRE_SUCCEEDED(TestType::get_nothrow(hkey, TestType::testValueName(), &result));
                REQUIRE(result == value);

                // and verify default value name
                result = {};
                REQUIRE_SUCCEEDED(TestType::set_nothrow(hkey, nullptr, value));
                REQUIRE_SUCCEEDED(TestType::get_nothrow(hkey, nullptr, &result));
                REQUIRE(result == value);
            }

            // fail get* if the value doesn't exist
            typename TestType::RetType result{};
            HRESULT hr = TestType::get_nothrow(hkey, invalidValueName, &result);
            REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
            REQUIRE(wil::reg::is_registry_not_found(hr));

            // fail if get* requests the wrong type
            for (auto& setWrongTypeFn : TestType::set_wrong_value_fns_openkey())
            {
                REQUIRE_SUCCEEDED(setWrongTypeFn(hkey, wrongTypeValueName));
                hr = TestType::get_nothrow(hkey, wrongTypeValueName, &result);
                REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
            }
        }

        SECTION("with string key")
        {
            for (auto&& value : TestType::testValues())
            {
                typename TestType::RetType result{};
                REQUIRE_SUCCEEDED(TestType::set_nothrow(HKEY_CURRENT_USER, testSubkey, TestType::testValueName(), value));
                REQUIRE_SUCCEEDED(TestType::get_nothrow(HKEY_CURRENT_USER, testSubkey, TestType::testValueName(), &result));
                REQUIRE(result == value);

                // verify reusing the previously allocated buffer
                REQUIRE_SUCCEEDED(TestType::get_nothrow(HKEY_CURRENT_USER, testSubkey, TestType::testValueName(), &result));
                REQUIRE(result == value);

                // and verify default value name
                result = {};
                REQUIRE_SUCCEEDED(TestType::set_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
                REQUIRE_SUCCEEDED(TestType::get_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
                REQUIRE(result == value);
            }

            // fail get* if the value doesn't exist
            typename TestType::RetType result{};
            HRESULT hr = TestType::get_nothrow(HKEY_CURRENT_USER, testSubkey, invalidValueName, &result);
            REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
            REQUIRE(wil::reg::is_registry_not_found(hr));

            // fail if get* requests the wrong type
            for (auto& setWrongTypeFn : TestType::set_wrong_value_fns_subkey())
            {
                REQUIRE_SUCCEEDED(setWrongTypeFn(HKEY_CURRENT_USER, testSubkey, wrongTypeValueName));
                hr = TestType::get_nothrow(HKEY_CURRENT_USER, testSubkey, wrongTypeValueName, &result);
                REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
            }
        }
    }
}

#if defined(WIL_ENABLE_EXCEPTIONS)
TEMPLATE_LIST_TEST_CASE("BasicRegistryTests::simple types typed gets/sets/try_gets", "[registry]", ThrowingTypesToTest)
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("get")
    {
        SECTION("with opened key")
        {
            wil::unique_hkey hkey;
            REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

            for (auto&& value : TestType::testValues())
            {
                TestType::set(hkey, TestType::testValueName(), value);
                auto result = TestType::get(hkey, TestType::testValueName());
                REQUIRE(result == value); // intentional fail

                // and verify default value name
                TestType::set(hkey, nullptr, value);
                result = TestType::get(hkey, nullptr);
                REQUIRE(result == value);
            }

            // fail if get* requests an invalid value
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]() {
                TestType::get(hkey, invalidValueName);
            });

            // fail if get* requests the wrong type
            for (auto& setWrongTypeFn : TestType::set_wrong_value_fns_openkey())
            {
                REQUIRE_SUCCEEDED(setWrongTypeFn(hkey, wrongTypeValueName));
                VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
                    TestType::get(hkey, wrongTypeValueName);
                });
            }
        }

        SECTION("with string key")
        {
            for (auto&& value : TestType::testValues())
            {
                TestType::set(HKEY_CURRENT_USER, testSubkey, TestType::testValueName(), value);
                auto result = TestType::get(HKEY_CURRENT_USER, testSubkey, TestType::testValueName());
                REQUIRE(result == value); // intentional fail

                // and verify default value name
                TestType::set(HKEY_CURRENT_USER, testSubkey, nullptr, value);
                result = TestType::get(HKEY_CURRENT_USER, testSubkey, nullptr);
                REQUIRE(result == value);
            }

            // fail if get* requests an invalid value
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]() {
                TestType::get(HKEY_CURRENT_USER, testSubkey, invalidValueName);
            });

            // fail if get* requests the wrong type
            for (auto& setWrongTypeFn : TestType::set_wrong_value_fns_subkey())
            {
                REQUIRE_SUCCEEDED(setWrongTypeFn(HKEY_CURRENT_USER, testSubkey, wrongTypeValueName));
                VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
                    TestType::get(HKEY_CURRENT_USER, testSubkey, wrongTypeValueName);
                });
            }
        }
    }

#if defined(__cpp_lib_optional)
    SECTION("try_get")
    {
        SECTION("with opened key")
        {
            wil::unique_hkey hkey;
            REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

            for (auto&& value : TestType::testValues())
            {
                TestType::set(hkey, TestType::testValueName(), value);
                auto result = TestType::try_get(hkey, TestType::testValueName());
                REQUIRE(result.value() == value); // intentional fail

                // and verify default value name
                TestType::set(hkey, nullptr, value);
                result = TestType::try_get(hkey, nullptr);
                REQUIRE(result.value() == value);
            }

            // try_get should simply return nullopt
            const auto result = TestType::try_get(hkey, invalidValueName);
            REQUIRE(!result.has_value());

            // fail if try_get* requests the wrong type
            for (auto& setWrongTypeFn : TestType::set_wrong_value_fns_openkey())
            {
                REQUIRE_SUCCEEDED(setWrongTypeFn(hkey, wrongTypeValueName));
                VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
                    TestType::try_get(hkey, wrongTypeValueName);
                });
            }
        }

        SECTION("with string key")
        {
            for (auto&& value : TestType::testValues())
            {
                TestType::set(HKEY_CURRENT_USER, testSubkey, TestType::testValueName(), value);
                auto result = TestType::try_get(HKEY_CURRENT_USER, testSubkey, TestType::testValueName());
                REQUIRE(result.value() == value); // intentional fail

                // and verify default value name
                TestType::set(HKEY_CURRENT_USER, testSubkey, nullptr, value);
                result = TestType::try_get(HKEY_CURRENT_USER, testSubkey, nullptr);
                REQUIRE(result.value() == value);
            }

            // try_get should simply return nullopt
            const auto result = TestType::try_get(HKEY_CURRENT_USER, testSubkey, invalidValueName);
            REQUIRE(!result.has_value());

            // fail if try_get* requests the wrong type
            for (auto& setWrongTypeFn : TestType::set_wrong_value_fns_subkey())
            {
                REQUIRE_SUCCEEDED(setWrongTypeFn(HKEY_CURRENT_USER, testSubkey, wrongTypeValueName));
                VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
                    TestType::try_get(HKEY_CURRENT_USER, testSubkey, wrongTypeValueName);
                });
            }
        }
    }
#endif // defined(__cpp_lib_optional)
}
#endif // defined(WIL_ENABLE_EXCEPTIONS)

#if defined(WIL_ENABLE_EXCEPTIONS)
TEST_CASE("BasicRegistryTests::wstrings", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

#if defined(_VECTOR_)
    SECTION("get_value_nothrow with non-null-terminated string: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        wil::reg::set_value_binary(hkey.get(), stringValueName, REG_SZ, nonNullTerminatedString);

        std::wstring result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
        REQUIRE(result == nonNullTerminatedStringFixed);
    }
    SECTION("get_value_nothrow with non-null-terminated string: with string key")
    {
        wil::reg::set_value_binary(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_SZ, nonNullTerminatedString);

        std::wstring result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result == nonNullTerminatedStringFixed);
    }
    SECTION("get_value_string with non-null-terminated string: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        wil::reg::set_value_binary(hkey.get(), stringValueName, REG_SZ, nonNullTerminatedString);

        const std::wstring result{wil::reg::get_value_string(hkey.get(), stringValueName)};
        REQUIRE(result == nonNullTerminatedStringFixed);
    }
    SECTION("get_value_string with non-null-terminated string: with string key")
    {
        wil::reg::set_value_binary(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_SZ, nonNullTerminatedString);

        const std::wstring result{wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName)};
        REQUIRE(result == nonNullTerminatedStringFixed);
    }

    SECTION("get_value_nothrow with empty string value: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        wil::reg::set_value_binary(hkey.get(), stringValueName, REG_SZ, emptyStringTestValue);

        std::wstring result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
        REQUIRE(result.empty());
    }
    SECTION("get_value_nothrow with empty string value: with string key")
    {
        wil::reg::set_value_binary(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_SZ, emptyStringTestValue);

        std::wstring result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result.empty());
    }
    SECTION("get_value_string with empty string value: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        wil::reg::set_value_binary(hkey.get(), stringValueName, REG_SZ, emptyStringTestValue);

        const std::wstring result{wil::reg::get_value_string(hkey.get(), stringValueName)};
        REQUIRE(result.empty());
    }
    SECTION("get_value_string with empty string value: with string key")
    {
        wil::reg::set_value_binary(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_SZ, emptyStringTestValue);

        const std::wstring result{wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName)};
        REQUIRE(result.empty());
    }
#endif

    SECTION("set_value_nothrow/get_value_string_nothrow: into buffers with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            WCHAR result[test_expanded_string_buffer_size]{};
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, value.c_str()));
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value.c_str()));
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), nullptr, result));
            REQUIRE(result == value);
        }

        WCHAR too_small_result[4]{};
        // fail get* if the buffer is too small
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, L"Test"));
        DWORD expectedSize_dword{};
        auto hr = wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, too_small_result, &expectedSize_dword);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA));
        REQUIRE(wil::reg::is_registry_buffer_too_small(hr));
        REQUIRE(expectedSize_dword == 12);
        WCHAR valid_buffer_result[5]{};
        unsigned int expectedSize_int{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, valid_buffer_result, &expectedSize_int));
        REQUIRE(expectedSize_int == 10);
        REQUIRE(0 == wcscmp(valid_buffer_result, L"Test"));

        // fail get* if the value doesn't exist
        uint32_t expectedSize_uint32{};
        hr = wil::reg::get_value_string_nothrow(hkey.get(), invalidValueName, too_small_result, &expectedSize_uint32);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));
        REQUIRE(expectedSize_uint32 == 0);

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(hkey.get(), dwordValueName, too_small_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_string_nothrow: into buffers with string key")
    {
        for (const auto& value : stringTestArray)
        {
            WCHAR result[test_expanded_string_buffer_size]{};
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(result == value);
        }

        WCHAR too_small_result[4]{};
        // fail get* if the buffer is too small
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, L"Test"));
        uint32_t expectedSize{};
        auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA));
        REQUIRE(wil::reg::is_registry_buffer_too_small(hr));
        REQUIRE(expectedSize == 12); // yes, this is a registry oddity that it returned 2-bytes-more-than-required
        WCHAR valid_buffer_result[5]{};
        REQUIRE_SUCCEEDED(
            wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, valid_buffer_result, &expectedSize));
        REQUIRE(expectedSize == 10);
        REQUIRE(0 == wcscmp(valid_buffer_result, L"Test"));

        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, invalidValueName, too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));
        REQUIRE(expectedSize == 0);

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, too_small_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
}
#endif

namespace
{
// Test string types across nothrow get, get, and try_get *and* generic get
// (get_value) vs typed get (get_value_string).
//
// This is similar to the templated tests used for simple types, but with a
// different matrix "flattening" strategy and test strategy---there are
// separate tests for generic gets vs typed gets, rather than separate
// generic/typed "structs" passed in.
//
// It was simply slightly easier to write the tests this way, and it makes
// it easier to special-case certain string types (eg wil::unique_* strings
// cannot be used with try_get because it becomes nearly impossible to
// actually *get* the value out of the result optional).
//
// This format is used similarly for expanded strings and binary getters
// below.

template <typename StringT, typename SetStringT = PCWSTR>
void verify_string_nothrow(
    std::function<HRESULT(PCWSTR, typename type_identity<StringT>::type&)> getFn,
    std::function<HRESULT(PCWSTR, typename type_identity<SetStringT>::type)> setFn,
    std::function<HRESULT(PCWSTR)> wrongSetFn)
{
    for (const auto& value : stringTestArray)
    {
        StringT result{};
        REQUIRE_SUCCEEDED(setFn(stringValueName, value.c_str()));
        REQUIRE_SUCCEEDED(getFn(stringValueName, result));
        REQUIRE(AreStringsEqual(result, value));

        // verify reusing the previously allocated buffer
        REQUIRE_SUCCEEDED(getFn(stringValueName, result));
        REQUIRE(AreStringsEqual(result, value));

        // and verify default value name
        result = {};
        REQUIRE_SUCCEEDED(setFn(nullptr, value.c_str()));
        REQUIRE_SUCCEEDED(getFn(nullptr, result));
        REQUIRE(AreStringsEqual(result, value));
    }

    // fail get* if the value doesn't exist
    StringT result{};
    HRESULT hr = getFn(invalidValueName, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    REQUIRE(wil::reg::is_registry_not_found(hr));

    // fail if get* requests the wrong type
    REQUIRE_SUCCEEDED(wrongSetFn(dwordValueName));
    hr = getFn(dwordValueName, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
}

template <typename StringT>
void verify_string_nothrow(HKEY key)
{
    verify_string_nothrow<StringT>(
        [&](PCWSTR valueName, StringT& output) {
            return wil::reg::get_value_string_nothrow(key, valueName, output);
        },
        [&](PCWSTR valueName, PCWSTR input) {
            return wil::reg::set_value_string_nothrow(key, valueName, input);
        },
        [&](PCWSTR valueName) -> HRESULT {
            return wil::reg::set_value_dword_nothrow(key, valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_string_nothrow(HKEY key, PCWSTR subkey)
{
    verify_string_nothrow<StringT>(
        [&](PCWSTR valueName, StringT& output) {
            return wil::reg::get_value_string_nothrow(key, subkey, valueName, output);
        },
        [&](PCWSTR valueName, PCWSTR input) {
            return wil::reg::set_value_string_nothrow(key, subkey, valueName, input);
        },
        [&](PCWSTR valueName) -> HRESULT {
            return wil::reg::set_value_dword_nothrow(key, subkey, valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_string_generic_get_value_nothrow(HKEY key)
{
    verify_string_nothrow<StringT>(
        [&](PCWSTR valueName, StringT& output) {
            return wil::reg::get_value_nothrow(key, valueName, output);
        },
        [&](PCWSTR valueName, PCWSTR input) {
            return wil::reg::set_value_nothrow(key, valueName, input);
        },
        [&](PCWSTR valueName) -> HRESULT {
            return wil::reg::set_value_dword_nothrow(key, valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_string_generic_get_value_nothrow(HKEY key, PCWSTR subkey)
{
    verify_string_nothrow<StringT>(
        [&](PCWSTR valueName, StringT& output) {
            return wil::reg::get_value_nothrow(key, subkey, valueName, output);
        },
        [&](PCWSTR valueName, PCWSTR input) {
            return wil::reg::set_value_nothrow(key, subkey, valueName, input);
        },
        [&](PCWSTR valueName) -> HRESULT {
            return wil::reg::set_value_dword_nothrow(key, subkey, valueName, test_dword_zero);
        });
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename StringT, typename StringSetT = PCWSTR>
void verify_string(
    std::function<typename type_identity<StringT>::type(PCWSTR)> getFn,
    std::function<void(PCWSTR, typename type_identity<StringSetT>::type)> setFn,
    std::function<void(PCWSTR)> setWrongTypeFn)
{
    for (const auto& value : stringTestArray)
    {
        setFn(stringValueName, value.c_str());
        auto result = getFn(stringValueName);
        REQUIRE(AreStringsEqual(result, value));

        // and verify default value name
        setFn(nullptr, value.c_str());
        result = getFn(nullptr);
        REQUIRE(AreStringsEqual(result, value));
    }

    // fail get* if the value doesn't exist
    VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]() {
        getFn(invalidValueName);
    });

    // fail if get* requests the wrong type
    setWrongTypeFn(dwordValueName);
    VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
        getFn(dwordValueName);
    });
}

template <typename StringT>
void verify_string()
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

    verify_string<StringT>(
        [&](PCWSTR valueName) {
            return wil::reg::get_value_string<StringT>(hkey.get(), valueName);
        },
        [&](PCWSTR valueName, PCWSTR value) -> void {
            wil::reg::set_value_string(hkey.get(), valueName, value);
        },
        [&](PCWSTR valueName) {
            wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_string_subkey()
{
    verify_string<StringT>(
        [](PCWSTR valueName) {
            return wil::reg::get_value_string<StringT>(HKEY_CURRENT_USER, testSubkey, valueName);
        },
        [](PCWSTR valueName, PCWSTR value) -> void {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, valueName, value);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_string_generic_get_value()
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

    verify_string<StringT>(
        [&](PCWSTR valueName) {
            return wil::reg::get_value<StringT>(hkey.get(), valueName);
        },
        [&](PCWSTR valueName, PCWSTR value) -> void {
            wil::reg::set_value(hkey.get(), valueName, value);
        },
        [&](PCWSTR valueName) {
            wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_string_generic_get_value_subkey()
{
    verify_string<StringT>(
        [](PCWSTR valueName) {
            return wil::reg::get_value<StringT>(HKEY_CURRENT_USER, testSubkey, valueName);
        },
        [](PCWSTR valueName, PCWSTR value) -> void {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, valueName, value);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}

#if defined(__cpp_lib_optional)
template <typename StringT, typename StringSetT = PCWSTR>
void verify_try_string(
    std::function<std::optional<StringT>(PCWSTR)> tryGetFn,
    std::function<void(PCWSTR, typename type_identity<StringSetT>::type)> setFn,
    std::function<void(PCWSTR)> setWrongTypeFn)
{
    for (const auto& value : stringTestArray)
    {
        setFn(stringValueName, value.c_str());
        auto result = tryGetFn(stringValueName);
        REQUIRE(AreStringsEqual(result.value(), value));

        // and verify default value name
        setFn(nullptr, value.c_str());
        result = tryGetFn(nullptr);
        REQUIRE(AreStringsEqual(result.value(), value));
    }

    // try_get should simply return nullopt
    const auto result = tryGetFn(invalidValueName);
    REQUIRE(!result.has_value());

    // fail if get* requests the wrong type
    setWrongTypeFn(dwordValueName);
    VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
        tryGetFn(dwordValueName);
    });
}

template <typename StringT>
void verify_try_string()
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

    verify_try_string<StringT>(
        [&](PCWSTR valueName) {
            return wil::reg::try_get_value_string<StringT>(hkey.get(), valueName);
        },
        [&](PCWSTR valueName, PCWSTR value) -> void {
            wil::reg::set_value_string(hkey.get(), valueName, value);
        },
        [&](PCWSTR valueName) {
            wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_try_string_subkey()
{
    verify_try_string<StringT>(
        [](PCWSTR valueName) {
            return wil::reg::try_get_value_string<StringT>(HKEY_CURRENT_USER, testSubkey, valueName);
        },
        [](PCWSTR valueName, PCWSTR value) -> void {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, valueName, value);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_try_string_generic_get_value()
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

    verify_try_string<StringT>(
        [&](PCWSTR valueName) {
            return wil::reg::try_get_value<StringT>(hkey.get(), valueName);
        },
        [&](PCWSTR valueName, PCWSTR value) -> void {
            wil::reg::set_value(hkey.get(), valueName, value);
        },
        [&](PCWSTR valueName) {
            wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_try_string_generic_get_value_subkey()
{
    verify_try_string<StringT>(
        [](PCWSTR valueName) {
            return wil::reg::try_get_value<StringT>(HKEY_CURRENT_USER, testSubkey, valueName);
        },
        [](PCWSTR valueName, PCWSTR value) -> void {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, valueName, value);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}
#endif // defined(__cpp_lib_optional)
#endif
} // namespace

TEST_CASE("BasicRegistryTests::string types", "[registry]")
{
    SECTION("set_value_string_nothrow/get_value_string_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // tests for set_value with PCWSTR values
        WCHAR pcwstr_result[test_expanded_string_buffer_size]{};
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, test_null_terminated_string));
        REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, pcwstr_result));
        REQUIRE(wcslen(pcwstr_result) == wcslen(test_null_terminated_string));
        REQUIRE(wcscmp(pcwstr_result, test_null_terminated_string) == 0);

        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, test_empty_null_terminated_string));
        REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, pcwstr_result));
        REQUIRE(wcslen(pcwstr_result) == wcslen(test_empty_null_terminated_string));
        REQUIRE(wcscmp(pcwstr_result, test_empty_null_terminated_string) == 0);

#if defined(__WIL_OLEAUTO_H_)
        verify_string_nothrow<wil::unique_bstr>(hkey.get());
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string_nothrow<wil::shared_bstr>(hkey.get());
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_string_nothrow<wil::unique_cotaskmem_string>(hkey.get());
#if defined(__WIL_OBJBASE_H_STL)
        verify_string_nothrow<wil::shared_cotaskmem_string>(hkey.get());
#endif
#endif
    }

    SECTION("set_value_string_nothrow/get_value_string_nothrow: with string key")
    {
        // tests for set_value with PCWSTR values
        WCHAR pcwstr_result[test_expanded_string_buffer_size]{};
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, test_null_terminated_string));
        REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, pcwstr_result));
        REQUIRE(wcslen(pcwstr_result) == wcslen(test_null_terminated_string));
        REQUIRE(wcscmp(pcwstr_result, test_null_terminated_string) == 0);

        REQUIRE_SUCCEEDED(
            wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, test_empty_null_terminated_string));
        REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, pcwstr_result));
        REQUIRE(wcslen(pcwstr_result) == wcslen(test_empty_null_terminated_string));
        REQUIRE(wcscmp(pcwstr_result, test_empty_null_terminated_string) == 0);

#if defined(__WIL_OLEAUTO_H_)
        verify_string_nothrow<wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey);
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string_nothrow<wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey);
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_string_nothrow<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey);
#if defined(__WIL_OBJBASE_H_STL)
        verify_string_nothrow<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey);
#endif
#endif
    }

    SECTION("strings set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // tests for set_value with PCWSTR values
        WCHAR pcwstr_result[test_expanded_string_buffer_size]{};
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, test_null_terminated_string));
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, pcwstr_result));
        REQUIRE(wcslen(pcwstr_result) == wcslen(test_null_terminated_string));
        REQUIRE(wcscmp(pcwstr_result, test_null_terminated_string) == 0);

        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, test_empty_null_terminated_string));
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, pcwstr_result));
        REQUIRE(wcslen(pcwstr_result) == wcslen(test_empty_null_terminated_string));
        REQUIRE(wcscmp(pcwstr_result, test_empty_null_terminated_string) == 0);

#if defined(__WIL_OLEAUTO_H_)
        verify_string_generic_get_value_nothrow<wil::unique_bstr>(hkey.get());
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string_generic_get_value_nothrow<wil::shared_bstr>(hkey.get());
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_string_generic_get_value_nothrow<wil::unique_cotaskmem_string>(hkey.get());
#if defined(__WIL_OBJBASE_H_STL)
        verify_string_generic_get_value_nothrow<wil::shared_cotaskmem_string>(hkey.get());
#endif
#endif
    }

    SECTION("strings set_value_nothrow/get_value_nothrow: with string key")
    {
        // tests for set_value with PCWSTR values
        WCHAR pcwstr_result[test_expanded_string_buffer_size]{};
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, test_null_terminated_string));
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, pcwstr_result));
        REQUIRE(wcslen(pcwstr_result) == wcslen(test_null_terminated_string));
        REQUIRE(wcscmp(pcwstr_result, test_null_terminated_string) == 0);

        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, test_empty_null_terminated_string));
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, pcwstr_result));
        REQUIRE(wcslen(pcwstr_result) == wcslen(test_empty_null_terminated_string));
        REQUIRE(wcscmp(pcwstr_result, test_empty_null_terminated_string) == 0);

#if defined(__WIL_OLEAUTO_H_)
        verify_string_generic_get_value_nothrow<wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey);
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string_generic_get_value_nothrow<wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey);
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_string_generic_get_value_nothrow<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey);
#if defined(__WIL_OBJBASE_H_STL)
        verify_string_generic_get_value_nothrow<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey);
#endif
#endif
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("set_value_string/get_value_string: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // tests for set_value with PCWSTR values
        wil::reg::set_value_string(hkey.get(), stringValueName, test_null_terminated_string);
        auto pcwstr_result = wil::reg::get_value_string(hkey.get(), stringValueName);
        REQUIRE(pcwstr_result.size() == wcslen(test_null_terminated_string));
        REQUIRE(pcwstr_result == test_null_terminated_string);

        wil::reg::set_value_string(hkey.get(), stringValueName, test_empty_null_terminated_string);
        pcwstr_result = wil::reg::get_value_string(hkey.get(), stringValueName);
        REQUIRE(pcwstr_result.size() == wcslen(test_empty_null_terminated_string));
        REQUIRE(pcwstr_result == test_empty_null_terminated_string);

#if defined(__WIL_OLEAUTO_H_)
        verify_string<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_string<wil::unique_cotaskmem_string>();
#if defined(__WIL_OBJBASE_H_STL)
        verify_string<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("set_value_string/get_value_string: with string key")
    {
        // tests for set_value with PCWSTR values
        wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, test_null_terminated_string);
        auto pcwstr_result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(pcwstr_result.size() == wcslen(test_null_terminated_string));
        REQUIRE(pcwstr_result == test_null_terminated_string);

        wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, test_empty_null_terminated_string);
        pcwstr_result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(pcwstr_result.size() == wcslen(test_empty_null_terminated_string));
        REQUIRE(pcwstr_result == test_empty_null_terminated_string);

#if defined(__WIL_OLEAUTO_H_)
        verify_string_subkey<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string_subkey<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_string_subkey<wil::unique_cotaskmem_string>();
#if defined(__WIL_OBJBASE_H_STL)
        verify_string_subkey<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("strings set_value/get_value: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // tests for set_value with PCWSTR values
        wil::reg::set_value(hkey.get(), stringValueName, test_null_terminated_string);
        auto pcwstr_result = wil::reg::get_value<std::wstring>(hkey.get(), stringValueName);
        REQUIRE(pcwstr_result.size() == wcslen(test_null_terminated_string));
        REQUIRE(pcwstr_result == test_null_terminated_string);

        wil::reg::set_value(hkey.get(), stringValueName, test_empty_null_terminated_string);
        pcwstr_result = wil::reg::get_value<std::wstring>(hkey.get(), stringValueName);
        REQUIRE(pcwstr_result.size() == wcslen(test_empty_null_terminated_string));
        REQUIRE(pcwstr_result == test_empty_null_terminated_string);

#if defined(__WIL_OLEAUTO_H_)
        verify_string_generic_get_value<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string_generic_get_value<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_string_generic_get_value<wil::unique_cotaskmem_string>();
#if defined(__WIL_OBJBASE_H_STL)
        verify_string_generic_get_value<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("strings set_value/get_value: with string key")
    {
        // tests for set_value with PCWSTR values
        wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, test_null_terminated_string);
        auto pcwstr_result = wil::reg::get_value<std::wstring>(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(pcwstr_result.size() == wcslen(test_null_terminated_string));
        REQUIRE(pcwstr_result == test_null_terminated_string);

        wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, test_empty_null_terminated_string);
        pcwstr_result = wil::reg::get_value<std::wstring>(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(pcwstr_result.size() == wcslen(test_empty_null_terminated_string));
        REQUIRE(pcwstr_result == test_empty_null_terminated_string);

#if defined(__WIL_OLEAUTO_H_)
        verify_string_generic_get_value_subkey<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string_generic_get_value_subkey<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_string_generic_get_value_subkey<wil::unique_cotaskmem_string>();
#if defined(__WIL_OBJBASE_H_STL)
        verify_string_generic_get_value_subkey<wil::shared_cotaskmem_string>();
#endif
#endif
    }

#if defined(__cpp_lib_optional)
    SECTION("strings set_value_string/try_get_value_string: with open key")
    {
        verify_try_string<std::wstring>();

#if defined(__WIL_OLEAUTO_H_)
#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_string<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
#if defined(__WIL_OBJBASE_H_STL)
        verify_try_string<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("strings set_value_string/try_get_value_string: with string key")
    {
        verify_try_string_subkey<std::wstring>();

#if defined(__WIL_OLEAUTO_H_)
#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_string_subkey<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
#if defined(__WIL_OBJBASE_H_STL)
        verify_try_string_subkey<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("strings set_value/try_get_value: with open key")
    {
        verify_try_string_generic_get_value<std::wstring>();

#if defined(__WIL_OLEAUTO_H_)
        // must fail to compile try_* with wil::unique_bstr
        // verify_try_string_generic_get_value<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_string_generic_get_value<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        // must fail to compile try_* with wil::unique_cotaskmem_string
        // verify_try_string_generic_get_value<wil::unique_cotaskmem_string>();
#if defined(__WIL_OBJBASE_H_STL)
        verify_try_string_generic_get_value<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("strings set_value/try_get_value: with string key")
    {
        verify_try_string_generic_get_value_subkey<std::wstring>();

#if defined(__WIL_OLEAUTO_H_)
        // must fail to compile try_* with wil::unique_bstr
        // verify_try_string_generic_get_value_subkey<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_string_generic_get_value_subkey<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        // must fail to compile try_* with wil::unique_cotaskmem_string
        // verify_try_string_generic_get_value_subkey<wil::unique_cotaskmem_string>();
#if defined(__WIL_OBJBASE_H_STL)
        verify_try_string_generic_get_value_subkey<wil::shared_cotaskmem_string>();
#endif
#endif
    }
#endif // defined(__cpp_lib_optional)

#endif
}

TEST_CASE("BasicRegistryTests::expanded_wstring", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), stringValueName, value.c_str()));
            WCHAR result[test_expanded_string_buffer_size]{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(std::wstring(result) == std::wstring(expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), nullptr, value.c_str()));
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), nullptr, result));
            REQUIRE(std::wstring(result) == std::wstring(expanded_value));
        }

        WCHAR result[10]{};
        // fail get* if the buffer is too small
        REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), stringValueName, L"%WINDIR%"));
        DWORD expectedSize{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), stringValueName, result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA));
        REQUIRE(wil::reg::is_registry_buffer_too_small(hr));
        REQUIRE(expectedSize == 22);
        WCHAR valid_buffer_result[11]{};
        uint32_t expectedSize_int{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(
            HKEY_CURRENT_USER, testSubkey, stringValueName, valid_buffer_result, &expectedSize_int));
        REQUIRE(expectedSize_int == 22);

        WCHAR expanded_value[test_expanded_string_buffer_size]{};
        const auto expanded_result = ::ExpandEnvironmentStringsW(L"%WINDIR%", expanded_value, test_expanded_string_buffer_size);
        REQUIRE(expanded_result != ERROR_SUCCESS);
        REQUIRE(expanded_result < test_expanded_string_buffer_size);
        REQUIRE(0 == wcscmp(valid_buffer_result, expanded_value));

        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), invalidValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow: with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            WCHAR result[test_expanded_string_buffer_size]{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(std::wstring(result) == std::wstring(expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(std::wstring(result) == std::wstring(expanded_value));
        }

        WCHAR result[10]{};
        // fail get* if the buffer is too small
        REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, L"%WINDIR%"));
        DWORD expectedSize{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA));
        REQUIRE(wil::reg::is_registry_buffer_too_small(hr));
        REQUIRE(expectedSize == 22);

        uint32_t expectedSize_int{};
        WCHAR valid_buffer_result[11]{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(
            HKEY_CURRENT_USER, testSubkey, stringValueName, valid_buffer_result, &expectedSize_int));
        REQUIRE(expectedSize_int == 22);

        WCHAR expanded_value[test_expanded_string_buffer_size]{};
        const auto expanded_result = ::ExpandEnvironmentStringsW(L"%WINDIR%", expanded_value, test_expanded_string_buffer_size);
        REQUIRE(expanded_result != ERROR_SUCCESS);
        REQUIRE(expanded_result < test_expanded_string_buffer_size);
        REQUIRE(0 == wcscmp(valid_buffer_result, expanded_value));

        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, invalidValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(wil::reg::is_registry_not_found(hr));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
}

namespace
{
// Text expanded strings across all our different string types and all our
// calling patterns (nothrow get, get, try_get and opened key vs string
// subkey).
//
// This is very similar to our string tests above and our binary getters
// below, but we compare against the expanded string
// (ExpandEnvironmentStringsW).
//
// Note that expanded strings do not support generic get (you can't call
// wil::reg::get_value to get an expanded string---how would you specify
// that in the call?).

template <typename StringT, typename SetStringT = PCWSTR>
void verify_expanded_string_nothrow(
    std::function<HRESULT(PCWSTR, typename type_identity<StringT>::type&)> getFn,
    std::function<HRESULT(PCWSTR, typename type_identity<SetStringT>::type)> setFn,
    std::function<HRESULT(PCWSTR)> setWrongTypeFn)
{
    for (const auto& value : expandedStringTestArray)
    {
        // verify the expanded string
        WCHAR expanded_value[test_expanded_string_buffer_size]{};
        const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
        REQUIRE(expanded_result != ERROR_SUCCESS);
        REQUIRE(expanded_result < test_expanded_string_buffer_size);

        StringT result{};
        REQUIRE_SUCCEEDED(setFn(stringValueName, value.c_str()));
        REQUIRE_SUCCEEDED(getFn(stringValueName, result));
        REQUIRE(AreStringsEqual(result, expanded_value));

        // verify reusing the previously allocated buffer
        REQUIRE_SUCCEEDED(getFn(stringValueName, result));
        REQUIRE(AreStringsEqual(result, expanded_value));

        // and verify default value name
        result = {};
        REQUIRE_SUCCEEDED(setFn(nullptr, value.c_str()));
        REQUIRE_SUCCEEDED(getFn(nullptr, result));
        REQUIRE(AreStringsEqual(result, expanded_value));
    }

    // fail get* if the value doesn't exist
    StringT result{};
    auto hr = getFn(invalidValueName, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    REQUIRE(wil::reg::is_registry_not_found(hr));

    // fail if get* requests the wrong type
    REQUIRE_SUCCEEDED(setWrongTypeFn(dwordValueName));
    hr = getFn(dwordValueName, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
}

template <typename StringT>
void verify_expanded_string_nothrow()
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

    verify_expanded_string_nothrow<StringT>(
        [&](PCWSTR valueName, StringT& output) {
            return wil::reg::get_value_expanded_string_nothrow(hkey.get(), valueName, output);
        },
        [&](PCWSTR valueName, PCWSTR input) {
            return wil::reg::set_value_expanded_string_nothrow(hkey.get(), valueName, input);
        },
        [&](PCWSTR valueName) {
            return wil::reg::set_value_dword_nothrow(hkey.get(), valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_expanded_string_subkey_nothrow()
{
    verify_expanded_string_nothrow<StringT>(
        [](PCWSTR valueName, StringT& output) {
            return wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output);
        },
        [](PCWSTR valueName, PCWSTR input) {
            return wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input);
        },
        [](PCWSTR valueName) {
            return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}

template <typename StringT, typename SetStringT = PCWSTR>
void verify_expanded_string(
    std::function<typename type_identity<StringT>::type(PCWSTR)> getFn,
    std::function<void(PCWSTR, typename type_identity<SetStringT>::type)> setFn,
    std::function<void(PCWSTR)> setWrongTypeFn)
{
    for (const auto& value : expandedStringTestArray)
    {
        // verify the expanded string
        WCHAR expanded_value[test_expanded_string_buffer_size]{};
        const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
        REQUIRE(expanded_result != ERROR_SUCCESS);
        REQUIRE(expanded_result < test_expanded_string_buffer_size);

        setFn(stringValueName, value.c_str());
        auto result = getFn(stringValueName);
        REQUIRE(AreStringsEqual(result, expanded_value));

        // and verify default value name
        setFn(nullptr, value.c_str());
        result = getFn(nullptr);
        REQUIRE(AreStringsEqual(result, expanded_value));
    }

    // fail get* if the value doesn't exist
    VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]() {
        getFn(invalidValueName);
    });

    // fail if get* requests the wrong type
    setWrongTypeFn(dwordValueName);
    VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
        getFn(dwordValueName);
    });
}

#if defined(WIL_ENABLE_EXCEPTIONS)
void verify_expanded_string()
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

    verify_expanded_string<std::wstring>(
        [&](PCWSTR valueName) -> std::wstring {
            return wil::reg::get_value_expanded_string(hkey.get(), valueName);
        },
        [&](PCWSTR valueName, PCWSTR input) {
            wil::reg::set_value_expanded_string(hkey.get(), valueName, input);
        },
        [&](PCWSTR valueName) {
            wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero);
        });
}

void verify_expanded_string_subkey()
{
    verify_expanded_string<std::wstring>(
        [](PCWSTR valueName) -> std::wstring {
            return wil::reg::get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, valueName);
        },
        [](PCWSTR valueName, PCWSTR input) {
            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, valueName, input);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_expanded_string()
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

    verify_expanded_string<StringT>(
        [&](PCWSTR valueName) -> StringT {
            return wil::reg::get_value_expanded_string<StringT>(hkey.get(), valueName);
        },
        [&](PCWSTR valueName, PCWSTR input) {
            wil::reg::set_value_expanded_string(hkey.get(), valueName, input);
        },
        [&](PCWSTR valueName) {
            wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_expanded_string_subkey()
{
    verify_expanded_string<StringT>(
        [](PCWSTR valueName) -> StringT {
            return wil::reg::get_value_expanded_string<StringT>(HKEY_CURRENT_USER, testSubkey, valueName);
        },
        [](PCWSTR valueName, PCWSTR input) {
            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, valueName, input);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}

#if defined(__cpp_lib_optional)
template <typename StringT, typename SetStringT = PCWSTR>
void verify_try_expanded_string(
    std::function<std::optional<typename type_identity<StringT>::type>(PCWSTR)> getFn,
    std::function<void(PCWSTR, typename type_identity<SetStringT>::type)> setFn,
    std::function<void(PCWSTR)> setWrongTypeFn)
{
    for (const auto& value : stringTestArray)
    {
        // verify the expanded string
        WCHAR expanded_value[test_expanded_string_buffer_size]{};
        const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
        REQUIRE(expanded_result != ERROR_SUCCESS);
        REQUIRE(expanded_result < test_expanded_string_buffer_size);

        setFn(stringValueName, value.c_str());
        auto result = getFn(stringValueName);
        REQUIRE(AreStringsEqual(result.value(), expanded_value));

        // and verify default value name
        setFn(nullptr, value.c_str());
        result = getFn(nullptr);
        REQUIRE(AreStringsEqual(result.value(), expanded_value));
    }

    // fail get* if the value doesn't exist
    const auto result = getFn(invalidValueName);
    REQUIRE(!result.has_value());

    // fail if get* requests the wrong type
    setWrongTypeFn(dwordValueName);
    VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
        getFn(dwordValueName);
    });
}

void verify_try_expanded_string()
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

    verify_try_expanded_string<std::wstring>(
        [&](PCWSTR valueName) {
            return wil::reg::try_get_value_expanded_string(hkey.get(), valueName);
        },
        [&](PCWSTR valueName, PCWSTR input) {
            wil::reg::set_value_expanded_string(hkey.get(), valueName, input);
        },
        [&](PCWSTR valueName) {
            wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero);
        });
}

void verify_try_expanded_string_subkey()
{
    verify_try_expanded_string<std::wstring>(
        [](PCWSTR valueName) {
            return wil::reg::try_get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, valueName);
        },
        [](PCWSTR valueName, PCWSTR input) {
            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, valueName, input);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_try_expanded_string()
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

    verify_try_expanded_string<StringT>(
        [&](PCWSTR valueName) {
            return wil::reg::try_get_value_expanded_string<StringT>(hkey.get(), valueName);
        },
        [&](PCWSTR valueName, PCWSTR input) {
            wil::reg::set_value_expanded_string(hkey.get(), valueName, input);
        },
        [&](PCWSTR valueName) {
            wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero);
        });
}

template <typename StringT>
void verify_try_expanded_string_subkey()
{
    verify_try_expanded_string<StringT>(
        [](PCWSTR valueName) {
            return wil::reg::try_get_value_expanded_string<StringT>(HKEY_CURRENT_USER, testSubkey, valueName);
        },
        [](PCWSTR valueName, PCWSTR input) {
            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, valueName, input);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}
#endif // defined(__cpp_lib_optional)
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)
} // namespace

TEST_CASE("BasicRegistryTests::expanded_string", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow: with opened key")
    {
#if defined(__WIL_OLEAUTO_H_)
        verify_expanded_string_nothrow<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_expanded_string_nothrow<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OLEAUTO_H_)
        verify_expanded_string_nothrow<wil::unique_cotaskmem_string>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_expanded_string_nothrow<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow: with string key")
    {
#if defined(__WIL_OLEAUTO_H_)
        verify_expanded_string_subkey_nothrow<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_expanded_string_subkey_nothrow<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OLEAUTO_H_)
        verify_expanded_string_subkey_nothrow<wil::unique_cotaskmem_string>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_expanded_string_subkey_nothrow<wil::shared_cotaskmem_string>();
#endif
#endif
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("set_value_expanded_string/get_value_expanded_string: with opened key")
    {
        verify_expanded_string();
        verify_expanded_string<std::wstring>();

#if defined(__WIL_OLEAUTO_H_)
        verify_expanded_string<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_expanded_string<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OLEAUTO_H_)
        verify_expanded_string<wil::unique_cotaskmem_string>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_expanded_string<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("set_value_expanded_string/get_value_expanded_string: with string key")
    {
        verify_expanded_string_subkey();
        verify_expanded_string_subkey<std::wstring>();

#if defined(__WIL_OLEAUTO_H_)
        verify_expanded_string_subkey<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_expanded_string_subkey<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OLEAUTO_H_)
        verify_expanded_string_subkey<wil::unique_cotaskmem_string>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_expanded_string_subkey<wil::shared_cotaskmem_string>();
#endif
#endif
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value_expanded_string/try_get_value_expanded_string: with open key")
    {
        verify_try_expanded_string();
        verify_try_expanded_string<std::wstring>();

#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_expanded_string<wil::shared_bstr>();
#endif

#if defined(__WIL_OLEAUTO_H_)
#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_expanded_string<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("set_value_expanded_string/try_get_value_expanded_string: with string key")
    {
        verify_try_expanded_string_subkey();
        verify_try_expanded_string_subkey<std::wstring>();

#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_expanded_string_subkey<wil::shared_bstr>();
#endif

#if defined(__WIL_OLEAUTO_H_)
#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_expanded_string_subkey<wil::shared_cotaskmem_string>();
#endif
#endif
    }
#endif
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)
}

TEST_CASE("BasicRegistryTests::multi-strings", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

#if defined(__WIL_OBJBASE_H_)
    SECTION("set_value_nothrow/get_value_nothrow: empty array with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // create a raw buffer to write a single null character
        wil::unique_cotaskmem_array_ptr<BYTE> byteBufferArrayOfOne{static_cast<BYTE*>(CoTaskMemAlloc(2)), 2};
        byteBufferArrayOfOne[0] = 0x00;
        byteBufferArrayOfOne[1] = 0x00;
        *byteBufferArrayOfOne.size_address() = 2;

        REQUIRE_SUCCEEDED(wil::reg::set_value_binary_nothrow(hkey.get(), stringValueName, REG_MULTI_SZ, byteBufferArrayOfOne));

        wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(hkey.get(), stringValueName, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        // verify reusing the previously allocated buffer
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(hkey.get(), stringValueName, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        // and verify default value name
        result = {};
        REQUIRE_SUCCEEDED(wil::reg::set_value_binary_nothrow(hkey.get(), nullptr, REG_MULTI_SZ, byteBufferArrayOfOne));
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(hkey.get(), nullptr, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));
    }
    SECTION("set_value_multistring_nothrow/get_value_multistring_nothrow: empty array with string key")
    {
        // create a raw buffer to write a single null character
        wil::unique_cotaskmem_array_ptr<BYTE> byteBufferArrayOfOne{static_cast<BYTE*>(CoTaskMemAlloc(2)), 2};
        byteBufferArrayOfOne[0] = 0x00;
        byteBufferArrayOfOne[1] = 0x00;
        *byteBufferArrayOfOne.size_address() = 2;

        REQUIRE_SUCCEEDED(
            wil::reg::set_value_binary_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_MULTI_SZ, byteBufferArrayOfOne));

        wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        // verify reusing the previously allocated buffer
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        // and verify default value name
        result = {};
        REQUIRE_SUCCEEDED(wil::reg::set_value_binary_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, REG_MULTI_SZ, byteBufferArrayOfOne));
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));

        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
        REQUIRE(AreStringsEqual<1>(result, stringLiteralArrayOfOne));
    }

    SECTION("set_value_multistring_nothrow/get_value_multistring_nothrow: odd values with string key")
    {
        REQUIRE(multiStringRawTestVector.size() == multiStringRawExpectedValues.size());

        for (size_t i = 0; i < multiStringRawTestVector.size(); ++i)
        {
            const auto& test_value = multiStringRawTestVector[i];
            const auto& expected_value = multiStringRawExpectedValues[i];

            wil::unique_cotaskmem_array_ptr<BYTE> no_throw_test_value{
                static_cast<BYTE*>(CoTaskMemAlloc(test_value.size())), test_value.size()};
            memcpy_s(no_throw_test_value.get(), no_throw_test_value.size(), test_value.data(), test_value.size());
            REQUIRE_SUCCEEDED(wil::reg::set_value_binary_nothrow(
                HKEY_CURRENT_USER, testSubkey, stringValueName, REG_MULTI_SZ, no_throw_test_value));

            wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, expected_value));

            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, expected_value));
        }
    }
#endif // #define __WIL_OBJBASE_H_

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("set_value_multistring/get_value_multistring: odd values with string key")
    {
        REQUIRE(multiStringRawTestVector.size() == multiStringRawExpectedValues.size());

        for (size_t i = 0; i < multiStringRawTestVector.size(); ++i)
        {
            const auto& test_value = multiStringRawTestVector[i];
            const auto& expected_value = multiStringRawExpectedValues[i];

            wil::reg::set_value_binary(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_MULTI_SZ, test_value);
            std::vector<std::wstring> result = wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == expected_value);

            result = wil::reg::get_value<::std::vector<::std::wstring>>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == expected_value);
        }
    }

    SECTION("set_value_multistring/get_value_multistring: empty array with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single
        // empty string) thus the result should have one empty string
#ifdef __WIL_WINREG_STL
        const std::vector<std::wstring> arrayOfOne{L""};
        wil::reg::set_value_multistring(hkey.get(), stringValueName, test_multistring_empty);
        auto result = wil::reg::get_value_multistring(hkey.get(), stringValueName);
        REQUIRE(result == arrayOfOne);

        result = wil::reg::get_value<::std::vector<::std::wstring>>(hkey.get(), stringValueName);
        REQUIRE(result == arrayOfOne);

        // and verify default value name
        wil::reg::set_value_multistring(hkey.get(), nullptr, test_multistring_empty);
        result = wil::reg::get_value_multistring(hkey.get(), nullptr);
        REQUIRE(result == arrayOfOne);

        result = wil::reg::get_value<::std::vector<::std::wstring>>(hkey.get(), nullptr);
        REQUIRE(result == arrayOfOne);
#endif // #ifdef __WIL_WINREG_STL
    }
    SECTION("set_value_multistring/get_value_multistring: empty array with string key")
    {
        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single
        // empty string) thus the result should have one empty string
#ifdef __WIL_WINREG_STL
        const std::vector<std::wstring> arrayOfOne{L""};
        wil::reg::set_value_multistring(HKEY_CURRENT_USER, testSubkey, stringValueName, test_multistring_empty);
        auto result = wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == arrayOfOne);

        result = wil::reg::get_value<::std::vector<::std::wstring>>(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == arrayOfOne);

        // and verify default value name
        wil::reg::set_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr, test_multistring_empty);
        result = wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr);
        REQUIRE(result == arrayOfOne);

        result = wil::reg::get_value<::std::vector<::std::wstring>>(HKEY_CURRENT_USER, testSubkey, nullptr);
        REQUIRE(result == arrayOfOne);
#endif // #ifdef __WIL_WINREG_STL
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_multistring: empty array with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single
        // empty string) thus the result should have one empty string
#ifdef __WIL_WINREG_STL
        const std::vector<std::wstring> arrayOfOne{L""};
        wil::reg::set_value(hkey.get(), stringValueName, test_multistring_empty);
        auto result = wil::reg::try_get_value_multistring(hkey.get(), stringValueName);
        REQUIRE(result.value() == arrayOfOne);

        result = wil::reg::try_get_value<::std::vector<::std::wstring>>(hkey.get(), stringValueName);
        REQUIRE(result.value() == arrayOfOne);

        // and verify default value name
        wil::reg::set_value(hkey.get(), nullptr, test_multistring_empty);
        result = wil::reg::try_get_value_multistring(hkey.get(), nullptr);
        REQUIRE(result.value() == arrayOfOne);

        result = wil::reg::try_get_value<::std::vector<::std::wstring>>(hkey.get(), nullptr);
        REQUIRE(result.value() == arrayOfOne);
#endif // #ifdef __WIL_WINREG_STL
    }

    SECTION("set_value/try_get_value_multistring: empty array with string key")
    {
        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single
        // empty string) thus the result should have one empty string
#ifdef __WIL_WINREG_STL
        const std::vector<std::wstring> arrayOfOne{L""};
        wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, test_multistring_empty);
        auto result = wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result.value() == arrayOfOne);

        result = wil::reg::try_get_value<::std::vector<::std::wstring>>(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result.value() == arrayOfOne);

        // and verify default value name
        wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, test_multistring_empty);
        result = wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr);
        REQUIRE(result.value() == arrayOfOne);

        result = wil::reg::try_get_value<::std::vector<::std::wstring>>(HKEY_CURRENT_USER, testSubkey, nullptr);
        REQUIRE(result.value() == arrayOfOne);
#endif // #ifdef __WIL_WINREG_STL
    }
#endif
#endif
}

#if defined(__WIL_OBJBASE_H_)
void verify_cotaskmem_array_nothrow(
    std::function<HRESULT(PCWSTR, DWORD, wil::unique_cotaskmem_array_ptr<BYTE>&)> getFn,
    std::function<HRESULT(PCWSTR, DWORD, const wil::unique_cotaskmem_array_ptr<BYTE>&)> setFn,
    std::function<HRESULT(PCWSTR, uint32_t)> setDwordFn)
{
    PopulateCoTaskMemArrayTestCases();
    for (const auto& value : cotaskmemArrayBytesTestArray)
    {
        wil::unique_cotaskmem_array_ptr<BYTE> result{};
        REQUIRE_SUCCEEDED(setFn(stringValueName, REG_BINARY, value));
        REQUIRE_SUCCEEDED(getFn(stringValueName, REG_BINARY, result));

        REQUIRE(std::equal(result.cbegin(), result.cend(), value.cbegin()));

        // verify reusing the same allocated buffer
        REQUIRE_SUCCEEDED(getFn(stringValueName, REG_BINARY, result));
        REQUIRE(result.size() == value.size());
        REQUIRE(std::equal(result.cbegin(), result.cend(), value.cbegin()));

        // and verify default value name
        result = {};
        REQUIRE_SUCCEEDED(setFn(nullptr, REG_BINARY, value));
        REQUIRE_SUCCEEDED(getFn(nullptr, REG_BINARY, result));
        REQUIRE(result.size() == value.size());
        REQUIRE(std::equal(result.cbegin(), result.cend(), value.cbegin()));
    }

    // fail get* if the value doesn't exist
    wil::unique_cotaskmem_array_ptr<BYTE> result{};
    auto hr = getFn(invalidValueName, REG_BINARY, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    REQUIRE(wil::reg::is_registry_not_found(hr));

    // fail if get* requests the wrong type
    hr = getFn(stringValueName, REG_SZ, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    hr = getFn(stringValueName, REG_DWORD, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));

    // should succeed if we specify the correct type
    REQUIRE_SUCCEEDED(setDwordFn(dwordValueName, 0xffffffff));
    REQUIRE_SUCCEEDED(getFn(dwordValueName, REG_DWORD, result));
    REQUIRE(result.size() == 4);
    REQUIRE(result[0] == 0xff);
    REQUIRE(result[1] == 0xff);
    REQUIRE(result[2] == 0xff);
    REQUIRE(result[3] == 0xff);
}
#endif

#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
namespace
{
// Test byte vectors/binary getters. These tests are very similar to the
// string and expanded string tests: we test across nothrow get, get, and
// try_get.
//
// These binary getters are used differently than all other getters, though.
// Callers must specify a read type indicating what type they expect the
// value to be. They also cannot be called using generic get_value for that
// reason.

void verify_byte_vector_nothrow(
    std::function<HRESULT(PCWSTR, DWORD, wil::unique_cotaskmem_array_ptr<BYTE>&)> getFn,
    std::function<void(PCWSTR, DWORD, const std::vector<BYTE>&)> setFn,
    std::function<HRESULT(PCWSTR, uint32_t)> setDwordFn)
{
    for (const auto& value : vectorBytesTestArray)
    {
        wil::unique_cotaskmem_array_ptr<BYTE> result{};
        setFn(stringValueName, REG_BINARY, value);
        REQUIRE_SUCCEEDED(getFn(stringValueName, REG_BINARY, result));
        REQUIRE(AreStringsEqual(result, value));

        // verify reusing the same allocated buffer
        REQUIRE_SUCCEEDED(getFn(stringValueName, REG_BINARY, result));
        REQUIRE(AreStringsEqual(result, value));

        // and verify default value name
        result = {};
        setFn(nullptr, REG_BINARY, value);
        REQUIRE_SUCCEEDED(getFn(nullptr, REG_BINARY, result));
        REQUIRE(AreStringsEqual(result, value));
    }

    // fail get* if the value doesn't exist
    wil::unique_cotaskmem_array_ptr<BYTE> result{};
    auto hr = getFn(invalidValueName, REG_BINARY, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    REQUIRE(wil::reg::is_registry_not_found(hr));

    // fail if get* requests the wrong type
    hr = getFn(stringValueName, REG_SZ, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    hr = getFn(stringValueName, REG_DWORD, result);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));

    // should succeed if we specify the correct type
    REQUIRE_SUCCEEDED(setDwordFn(dwordValueName, 0xffffffff));
    REQUIRE_SUCCEEDED(getFn(dwordValueName, REG_DWORD, result));
    REQUIRE(result.size() == 4);
    REQUIRE(result[0] == 0xff);
    REQUIRE(result[1] == 0xff);
    REQUIRE(result[2] == 0xff);
    REQUIRE(result[3] == 0xff);
}

void verify_byte_vector(
    std::function<std::vector<BYTE>(PCWSTR, DWORD)> getFn,
    std::function<void(PCWSTR, DWORD, const std::vector<BYTE>&)> setFn,
    std::function<void(PCWSTR, uint32_t)> setDwordFn)
{
    for (const auto& value : vectorBytesTestArray)
    {
        setFn(stringValueName, REG_BINARY, value);
        auto result = getFn(stringValueName, REG_BINARY);
        REQUIRE(result == value);

        // and verify default value name
        setFn(nullptr, REG_BINARY, value);
        result = getFn(nullptr, REG_BINARY);
        REQUIRE(result == value);
    }

    // fail get* if the value doesn't exist
    VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]() {
        const auto return_value = getFn(invalidValueName, REG_BINARY);
    });

    // fail if get* requests the wrong type
    setDwordFn(dwordValueName, 0xffffffff);
    VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
        const auto return_value = getFn(dwordValueName, REG_BINARY);
    });

    // should succeed if we specify the correct type
    auto result = getFn(dwordValueName, REG_DWORD);
    REQUIRE(result.size() == 4);
    REQUIRE(result[0] == 0xff);
    REQUIRE(result[1] == 0xff);
    REQUIRE(result[2] == 0xff);
    REQUIRE(result[3] == 0xff);
}

#if defined(__cpp_lib_optional)
void verify_try_byte_vector(
    std::function<std::optional<std::vector<BYTE>>(PCWSTR, DWORD)> tryGetFn,
    std::function<void(PCWSTR, DWORD, const std::vector<BYTE>&)> setFn,
    std::function<void(PCWSTR, uint32_t)> setDwordFn)
{
    for (const auto& value : vectorBytesTestArray)
    {
        setFn(stringValueName, REG_BINARY, value);
        auto result = tryGetFn(stringValueName, REG_BINARY);
        REQUIRE(result == value);

        // and verify default value name
        setFn(nullptr, REG_BINARY, value);
        result = tryGetFn(nullptr, REG_BINARY);
        REQUIRE(result == value);
    }

    // fail get* if the value doesn't exist
    auto result = tryGetFn(invalidValueName, REG_BINARY);
    REQUIRE(!result.has_value());

    // fail if get* requests the wrong type
    setDwordFn(dwordValueName, 0xffffffff);
    VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]() {
        tryGetFn(dwordValueName, REG_BINARY);
    });

    // should succeed if we specify the correct type
    result = tryGetFn(dwordValueName, REG_DWORD);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 4);
    REQUIRE(result->at(0) == 0xff);
    REQUIRE(result->at(1) == 0xff);
    REQUIRE(result->at(2) == 0xff);
    REQUIRE(result->at(3) == 0xff);
}
#endif // #if defined(__cpp_lib_optional)
} // namespace

TEST_CASE("BasicRegistryTests::vector-bytes", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_binary/get_value_binary: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_byte_vector(
            [&](PCWSTR valueName, DWORD type) {
                return wil::reg::get_value_binary(hkey.get(), valueName, type);
            },
            [&](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) {
                wil::reg::set_value_binary(hkey.get(), valueName, type, input);
            },
            [&](PCWSTR valueName, DWORD input) {
                wil::reg::set_value_dword(hkey.get(), valueName, input);
            });

        verify_byte_vector_nothrow(
            [&](PCWSTR valueName, DWORD type, wil::unique_cotaskmem_array_ptr<BYTE>& value) {
                return wil::reg::get_value_binary_nothrow(hkey.get(), valueName, type, value);
            },
            [&](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) {
                wil::reg::set_value_binary(hkey.get(), valueName, type, input);
            },
            [&](PCWSTR valueName, DWORD input) {
                return wil::reg::set_value_dword_nothrow(hkey.get(), valueName, input);
            });
    }
    SECTION("set_value_binary/get_value_binary: with string key")
    {
        verify_byte_vector(
            [](PCWSTR valueName, DWORD type) {
                return wil::reg::get_value_binary(HKEY_CURRENT_USER, testSubkey, valueName, type);
            },
            [](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) {
                wil::reg::set_value_binary(HKEY_CURRENT_USER, testSubkey, valueName, type, input);
            },
            [](PCWSTR valueName, DWORD input) {
                wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, input);
            });

        verify_byte_vector_nothrow(
            [&](PCWSTR valueName, DWORD type, wil::unique_cotaskmem_array_ptr<BYTE>& value) {
                return wil::reg::get_value_binary_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, type, value);
            },
            [&](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) {
                wil::reg::set_value_binary(HKEY_CURRENT_USER, testSubkey, valueName, type, input);
            },
            [&](PCWSTR valueName, DWORD input) {
                return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input);
            });
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value_binary/try_get_value_binary: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_try_byte_vector(
            [&](PCWSTR valueName, DWORD type) {
                return wil::reg::try_get_value_binary(hkey.get(), valueName, type);
            },
            [&](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) {
                wil::reg::set_value_binary(hkey.get(), valueName, type, input);
            },
            [&](PCWSTR valueName, DWORD input) {
                wil::reg::set_value_dword(hkey.get(), valueName, input);
            });
    }

    SECTION("set_value/try_get_value_binary: with string key")
    {
        verify_try_byte_vector(
            [](PCWSTR valueName, DWORD type) {
                return wil::reg::try_get_value_binary(HKEY_CURRENT_USER, testSubkey, valueName, type);
            },
            [](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) {
                wil::reg::set_value_binary(HKEY_CURRENT_USER, testSubkey, valueName, type, input);
            },
            [](PCWSTR valueName, DWORD input) {
                wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, input);
            });
    }
#endif // #if defined(__cpp_lib_optional)
}
#endif // #if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_OBJBASE_H_)
TEST_CASE("BasicRegistryTests::cotaskmem_array-bytes", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_cotaskmem_array_byte_nothrow/get_value_binary_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_cotaskmem_array_nothrow(
            [&](PCWSTR valueName, DWORD type, wil::unique_cotaskmem_array_ptr<BYTE>& output) {
                return wil::reg::get_value_binary_nothrow(hkey.get(), valueName, type, output);
            },
            [&](PCWSTR valueName, DWORD type, const wil::unique_cotaskmem_array_ptr<BYTE>& input) {
                return wil::reg::set_value_binary_nothrow(hkey.get(), nullptr, valueName, type, input);
            },
            [&](PCWSTR valueName, DWORD input) {
                return wil::reg::set_value_dword_nothrow(hkey.get(), valueName, input);
            });
    }
    SECTION("set_value_cotaskmem_array_byte_nothrow/get_value_binary_nothrow: with string key")
    {
        verify_cotaskmem_array_nothrow(
            [](PCWSTR valueName, DWORD type, wil::unique_cotaskmem_array_ptr<BYTE>& output) {
                return wil::reg::get_value_binary_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, type, output);
            },
            [](PCWSTR valueName, DWORD type, const wil::unique_cotaskmem_array_ptr<BYTE>& input) {
                return wil::reg::set_value_binary_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, type, input);
            },
            [](PCWSTR valueName, DWORD input) {
                return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input);
            });
    }
}
#endif // #if defined(__WIL_OBJBASE_H_)

#if defined(WIL_ENABLE_EXCEPTIONS)
#if defined(_STRING_)
TEST_CASE("BasicRegistryTests::value_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("value_iterator with no values")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};

        auto test_iterator{wil::reg::value_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::value_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
    }

    SECTION("value_iterator with one value - manual iterator usage")
    {
        wil::unique_hkey write_hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(write_hkey.get(), test_enum_valueName1, 0);

        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};

        // both ways to access the iterator data
        auto test_iterator = wil::reg::value_iterator(hkey.get());
        REQUIRE(test_iterator == wil::reg::value_iterator(hkey.get()));
        REQUIRE((*test_iterator).name == test_enum_valueName1);
        REQUIRE(test_iterator->name == test_enum_valueName1);
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_iterator(hkey.get()));
        REQUIRE((*test_iterator_copy).name == test_enum_valueName1);
        REQUIRE(test_iterator_copy->name == test_enum_valueName1);
        ++test_iterator;
        REQUIRE(test_iterator == wil::reg::value_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_iterator{});

        test_iterator = wil::reg::value_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::value_iterator{});
        REQUIRE(test_iterator == wil::reg::value_iterator(hkey.get()));
        REQUIRE((*test_iterator).name == test_enum_valueName1);
        REQUIRE(test_iterator->name == test_enum_valueName1);
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_iterator(hkey.get()));
        REQUIRE((*test_iterator_copy).name == test_enum_valueName1);
        REQUIRE(test_iterator_copy->name == test_enum_valueName1);
        ++test_iterator;
        REQUIRE(test_iterator == wil::reg::value_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_iterator{});

        const wil::unique_hkey hkey2{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        test_iterator = wil::reg::value_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::value_iterator{});
        REQUIRE((*test_iterator).name == test_enum_valueName1);
        REQUIRE(test_iterator->name == test_enum_valueName1);
        test_iterator_copy = test_iterator;
        REQUIRE((*test_iterator_copy).name == test_enum_valueName1);
        REQUIRE(test_iterator_copy->name == test_enum_valueName1);
        test_iterator += 1;
        REQUIRE(test_iterator == wil::reg::value_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_iterator{});
    }

    SECTION("value_iterator changing the iterator data string sizes to force resize and trim")
    {
        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[0], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[1], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[2], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[3], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[4], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[5], L"");

        const auto begin = wil::reg::value_iterator{write_hkey.get()};
        const auto end = wil::reg::value_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto nameAndType) {
            auto stringLength = wcslen(nameAndType.name.c_str());
            REQUIRE(stringLength == nameAndType.name.size());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.c_str(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](const auto& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.c_str());
            REQUIRE(stringLength == nameAndType.name.size());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.c_str(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](auto&& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.c_str());
            REQUIRE(stringLength == nameAndType.name.size());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.c_str(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));
    }

    SECTION("value_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2 - 2;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length;

        std::wstring half_length_name(half_length, L'a');
        std::wstring full_length_name(full_length, L'b');

        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::set_value_string(write_hkey.get(), half_length_name.c_str(), half_length_name.c_str());
        wil::reg::set_value_string(write_hkey.get(), full_length_name.c_str(), full_length_name.c_str());

        auto iterate_values = wil::reg::value_iterator{write_hkey.get()};
        const auto& half_length_value = *iterate_values;
        REQUIRE(half_length_value.name == half_length_name);
        ++iterate_values;
        const auto& full_length_value = *iterate_values;
        REQUIRE(full_length_value.name == full_length_name);
        ++iterate_values;
        REQUIRE(iterate_values == wil::reg::value_iterator{});
    }

    SECTION("value_iterator with many values - std::for_each usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(hkey.get(), test_enum_valueName1, 0);
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1ul);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ull);
        wil::reg::set_value(hkey.get(), test_enum_valueName4, L"four");

        uint32_t count = 0;
        std::for_each(wil::reg::value_iterator(hkey.get()), wil::reg::value_iterator{}, [&](const auto& value_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(value_data.name == test_enum_valueName1);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(value_data.name == test_enum_valueName2);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(value_data.name == test_enum_valueName3);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(value_data.name == test_enum_valueName4);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        });
        REQUIRE(count == 4);

        count = 0;
        const auto testIterator = wil::reg::value_iterator(hkey.get());
        const auto testEndIterator = wil::reg::value_iterator{};
        std::for_each(testIterator, testEndIterator, [&](const auto& value_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(value_data.name == test_enum_valueName1);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(value_data.name == test_enum_valueName2);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(value_data.name == test_enum_valueName3);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(value_data.name == test_enum_valueName4);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        });
        REQUIRE(count == 4);

        auto std_count = std::count_if(
            wil::reg::value_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_iterator{},
            [&](const auto& value_data) {
                return (value_data.name == test_enum_valueName1) && (value_data.name.size() == wcslen(test_enum_valueName1)) &&
                       (value_data.type == REG_DWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_iterator{},
            [&](const auto& value_data) {
                return (value_data.name == test_enum_valueName2) && (value_data.name.size() == wcslen(test_enum_valueName2)) &&
                       (value_data.type == REG_DWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_iterator{},
            [&](const auto& value_data) {
                return (value_data.name == test_enum_valueName3) && (value_data.name.size() == wcslen(test_enum_valueName3)) &&
                       (value_data.type == REG_QWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_iterator{},
            [&](const auto& value_data) {
                return (value_data.name == test_enum_valueName4) && (value_data.name.size() == wcslen(test_enum_valueName4)) &&
                       (value_data.type == REG_SZ);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_iterator{},
            [&](const auto& value_data) {
                return (value_data.name == L"xyz");
            });
        REQUIRE(std_count == 0);
    }

    SECTION("value_iterator with many values - range-for iterator usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(hkey.get(), test_enum_valueName1, 0);
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1ul);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ull);
        wil::reg::set_value(hkey.get(), test_enum_valueName4, L"four");

        uint32_t count = 0;
        for (const auto& value_data : wil::make_range(wil::reg::value_iterator{hkey.get()}, wil::reg::value_iterator{}))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(value_data.name == test_enum_valueName1);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(value_data.name == test_enum_valueName2);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(value_data.name == test_enum_valueName3);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(value_data.name == test_enum_valueName4);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }

        count = 0;
        const auto testIterator = wil::reg::value_iterator(hkey.get());
        const auto testEndIterator = wil::reg::value_iterator{};
        for (const auto& value_data : wil::make_range(testIterator, testEndIterator))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(value_data.name == test_enum_valueName1);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(value_data.name == test_enum_valueName2);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(value_data.name == test_enum_valueName3);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(value_data.name == test_enum_valueName4);
                REQUIRE(value_data.name.size() == wcslen(test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE(count == 4);
    }
}

TEST_CASE("BasicRegistryTests::key_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("key_iterator with no subkeys")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};

        auto test_iterator{wil::reg::key_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::key_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
    }

    SECTION("key_iterator with one subkey - manual iterator usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::create_unique_key(hkey.get(), test_enum_KeyName1);

        const auto key_enum = wil::reg::key_iterator(hkey.get());
        const auto key_end = wil::reg::key_iterator{};
        REQUIRE(key_enum != key_end);

        auto test_iterator = wil::reg::key_iterator(hkey.get());
        REQUIRE(test_iterator == wil::reg::key_iterator(hkey.get()));
        REQUIRE((*test_iterator).name == test_enum_KeyName1);
        REQUIRE(test_iterator->name == test_enum_KeyName1);
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_iterator(hkey.get()));
        REQUIRE((*test_iterator_copy).name == test_enum_KeyName1);
        REQUIRE(test_iterator_copy->name == test_enum_KeyName1);
        ++test_iterator;
        REQUIRE(test_iterator == key_end);
        REQUIRE(test_iterator == wil::reg::key_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == key_end);
        REQUIRE(test_iterator_copy == wil::reg::key_iterator{});

        test_iterator = wil::reg::key_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::key_iterator{});
        REQUIRE(test_iterator == wil::reg::key_iterator(hkey.get()));
        REQUIRE((*test_iterator).name == test_enum_KeyName1);
        REQUIRE(test_iterator->name == test_enum_KeyName1);
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_iterator(hkey.get()));
        REQUIRE((*test_iterator_copy).name == test_enum_KeyName1);
        REQUIRE(test_iterator_copy->name == test_enum_KeyName1);
        ++test_iterator;
        REQUIRE(test_iterator == key_end);
        REQUIRE(test_iterator == wil::reg::key_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == key_end);
        REQUIRE(test_iterator_copy == wil::reg::key_iterator{});

        const wil::unique_hkey hkey2{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        test_iterator = wil::reg::key_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::key_iterator{});
        REQUIRE((*test_iterator).name == test_enum_KeyName1);
        REQUIRE(test_iterator->name == test_enum_KeyName1);
        test_iterator_copy = test_iterator;
        REQUIRE((*test_iterator_copy).name == test_enum_KeyName1);
        REQUIRE(test_iterator_copy->name == test_enum_KeyName1);
        test_iterator += 1;
        REQUIRE(test_iterator == wil::reg::key_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_iterator{});
    }

    SECTION("key_iterator changing the iterator data string sizes to force resize and trim")
    {
        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[0]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[1]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[2]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[3]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[4]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[5]);

        const auto begin = wil::reg::key_iterator{write_hkey.get()};
        const auto end = wil::reg::key_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto keyInfo) {
            auto stringLength = wcslen(keyInfo.name.c_str());
            REQUIRE(stringLength == keyInfo.name.size());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.c_str(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](const auto& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.c_str());
            REQUIRE(stringLength == keyInfo.name.size());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.c_str(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](auto&& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.c_str());
            REQUIRE(stringLength == keyInfo.name.size());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.c_str(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty
    }

    SECTION("key_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2 - 1;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length;

        std::wstring half_length_name(half_length, L'a');
        std::wstring full_length_name(full_length, L'b');

        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::create_unique_key(write_hkey.get(), half_length_name.c_str());
        wil::reg::create_unique_key(write_hkey.get(), full_length_name.c_str());

        auto iterate_keys = wil::reg::key_iterator{write_hkey.get()};
        const auto& half_length_value = *iterate_keys;
        REQUIRE(half_length_value.name == half_length_name);
        ++iterate_keys;
        const auto& full_length_value = *iterate_keys;
        REQUIRE(full_length_value.name == full_length_name);
        ++iterate_keys;
        REQUIRE(iterate_keys == wil::reg::key_iterator{});
    }

    SECTION("key_iterator with many subkeys - std::for_each and std::count usage")
    {
        wil::unique_hkey enum_hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName1);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName2);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName3);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName4);

        uint32_t count = 0;
        std::for_each(wil::reg::key_iterator{enum_hkey.get()}, wil::reg::key_iterator{}, [&](const auto& key_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(key_data.name == test_enum_KeyName1);
                REQUIRE(key_data.name.size() == wcslen(test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(key_data.name == test_enum_KeyName2);
                REQUIRE(key_data.name.size() == wcslen(test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(key_data.name == test_enum_KeyName3);
                REQUIRE(key_data.name.size() == wcslen(test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(key_data.name == test_enum_KeyName4);
                REQUIRE(key_data.name.size() == wcslen(test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        });
        REQUIRE(count == 4);

        auto std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, test_enum_KeyName1);
        REQUIRE(std_count == 1);

        std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, test_enum_KeyName2);
        REQUIRE(std_count == 1);

        std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, test_enum_KeyName3);
        REQUIRE(std_count == 1);

        std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, test_enum_KeyName4);
        REQUIRE(std_count == 1);

        std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, L"xyz");
        REQUIRE(std_count == 0);

#if defined(__WIL_WINREG_STL)
        // repeat with wil::shared_hkey
        std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, test_enum_KeyName1);
        REQUIRE(std_count == 1);

        std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, test_enum_KeyName2);
        REQUIRE(std_count == 1);

        std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, test_enum_KeyName3);
        REQUIRE(std_count == 1);

        std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, test_enum_KeyName4);
        REQUIRE(std_count == 1);

        std_count = std::count(
            wil::reg::key_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()), wil::reg::key_iterator{}, L"xyz");
        REQUIRE(std_count == 0);
#endif // #if defined(__WIL_WINREG_STL)
    }

    SECTION("key_iterator with many subkeys - range-for iterator usage")
    {
        wil::unique_hkey enum_hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName1);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName2);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName3);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName4);

        uint32_t count = 0;
        for (const auto& key_data : wil::make_range(wil::reg::key_iterator{enum_hkey.get()}, wil::reg::key_iterator{}))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(key_data.name == test_enum_KeyName1);
                REQUIRE(key_data.name.size() == wcslen(test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(key_data.name == test_enum_KeyName2);
                REQUIRE(key_data.name.size() == wcslen(test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(key_data.name == test_enum_KeyName3);
                REQUIRE(key_data.name.size() == wcslen(test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(key_data.name == test_enum_KeyName4);
                REQUIRE(key_data.name.size() == wcslen(test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
    }
}
#endif // #if !defined(_STRING_)

#if defined(__WIL_OLEAUTO_H_)
TEST_CASE("BasicRegistryTests::value_bstr_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("value_bstr_iterator with no values")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};

        auto test_iterator{wil::reg::value_bstr_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::value_bstr_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
    }

    SECTION("value_bstr_iterator with one value - manual iterator usage")
    {
        wil::unique_hkey write_hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(write_hkey.get(), test_enum_valueName1, 0);

        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};

        // both ways to access the iterator data
        auto test_iterator = wil::reg::value_bstr_iterator(hkey.get());
        REQUIRE(test_iterator == wil::reg::value_bstr_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        ++test_iterator;
        REQUIRE(test_iterator == wil::reg::value_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_iterator{});

        test_iterator = wil::reg::value_bstr_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::value_bstr_iterator{});
        REQUIRE(test_iterator == wil::reg::value_bstr_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        ++test_iterator;
        REQUIRE(test_iterator == wil::reg::value_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_iterator{});

        const wil::unique_hkey hkey2{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        test_iterator = wil::reg::value_bstr_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::value_bstr_iterator{});
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        test_iterator_copy = test_iterator;
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        test_iterator += 1;
        REQUIRE(test_iterator == wil::reg::value_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_iterator{});
    }

    SECTION("value_bstr_iterator changing the iterator data string sizes to force resize and trim")
    {
        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[0], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[1], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[2], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[3], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[4], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[5], L"");

        const auto begin = wil::reg::value_bstr_iterator{write_hkey.get()};
        const auto end = wil::reg::value_bstr_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](const auto& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](auto&& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));
    }

    SECTION("value_bstr_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2 - 2;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length;

        wil::unique_bstr half_length_name{SysAllocStringLen(nullptr, half_length)};
        REQUIRE(half_length_name);
        wil::unique_bstr full_length_name{SysAllocStringLen(nullptr, full_length)};
        REQUIRE(full_length_name);
        memset(half_length_name.get(), L'a', half_length * sizeof(wchar_t));
        memset(full_length_name.get(), L'b', full_length * sizeof(wchar_t));

        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::set_value_string(write_hkey.get(), half_length_name.get(), half_length_name.get());
        wil::reg::set_value_string(write_hkey.get(), full_length_name.get(), full_length_name.get());

        auto iterate_values = wil::reg::value_bstr_iterator{write_hkey.get()};
        const auto& half_length_value = *iterate_values;
        REQUIRE(half_length == SysStringLen(half_length_value.name.get()));
        REQUIRE(0 == wcscmp(half_length_value.name.get(), half_length_name.get()));
        ++iterate_values;
        const auto& full_length_value = *iterate_values;
        REQUIRE(full_length == SysStringLen(full_length_value.name.get()));
        REQUIRE(0 == wcscmp(full_length_value.name.get(), full_length_name.get()));
        ++iterate_values;
        REQUIRE(iterate_values == wil::reg::value_bstr_iterator{});
    }

    SECTION("value_bstr_iterator with many values - std::for_each usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(hkey.get(), test_enum_valueName1, 0);
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1ul);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ull);
        wil::reg::set_value(hkey.get(), test_enum_valueName4, L"four");

        uint32_t count = 0;
        std::for_each(wil::reg::value_bstr_iterator(hkey.get()), wil::reg::value_bstr_iterator{}, [&](const auto& value_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        });
        REQUIRE(count == 4);

        count = 0;
        const auto testIterator = wil::reg::value_bstr_iterator(hkey.get());
        const auto testEndIterator = wil::reg::value_bstr_iterator{};
        std::for_each(testIterator, testEndIterator, [&](const auto& value_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        });
        REQUIRE(count == 4);

        auto std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), test_enum_valueName1)) && (value_data.type == REG_DWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), test_enum_valueName2)) && (value_data.type == REG_DWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), test_enum_valueName3)) && (value_data.type == REG_QWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), test_enum_valueName4)) && (value_data.type == REG_SZ);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), L"xyz"));
            });
        REQUIRE(std_count == 0);
    }

    SECTION("value_bstr_iterator with many values - range-for iterator usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(hkey.get(), test_enum_valueName1, 0);
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1ul);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ull);
        wil::reg::set_value(hkey.get(), test_enum_valueName4, L"four");

        uint32_t count = 0;
        for (const auto& value_data : wil::make_range(wil::reg::value_bstr_iterator{hkey.get()}, wil::reg::value_bstr_iterator{}))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }

        count = 0;
        const auto testIterator = wil::reg::value_bstr_iterator(hkey.get());
        const auto testEndIterator = wil::reg::value_bstr_iterator{};
        for (const auto& value_data : wil::make_range(testIterator, testEndIterator))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE(count == 4);
    }
}

TEST_CASE("BasicRegistryTests::key_bstr_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("key_bstr_iterator with no subkeys")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};

        auto test_iterator{wil::reg::key_bstr_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::key_bstr_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
    }

    SECTION("key_bstr_iterator with one subkey - manual iterator usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::create_unique_key(hkey.get(), test_enum_KeyName1);

        const auto key_enum = wil::reg::key_bstr_iterator(hkey.get());
        const auto key_end = wil::reg::key_bstr_iterator{};
        REQUIRE(key_enum != key_end);

        auto test_iterator = wil::reg::key_bstr_iterator(hkey.get());
        REQUIRE(test_iterator == wil::reg::key_bstr_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_KeyName1));
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_bstr_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_KeyName1));
        ++test_iterator;
        REQUIRE(test_iterator == key_end);
        REQUIRE(test_iterator == wil::reg::key_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == key_end);
        REQUIRE(test_iterator_copy == wil::reg::key_bstr_iterator{});

        test_iterator = wil::reg::key_bstr_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::key_bstr_iterator{});
        REQUIRE(test_iterator == wil::reg::key_bstr_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_KeyName1));
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_bstr_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_KeyName1));
        ++test_iterator;
        REQUIRE(test_iterator == key_end);
        REQUIRE(test_iterator == wil::reg::key_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == key_end);
        REQUIRE(test_iterator_copy == wil::reg::key_bstr_iterator{});

        const wil::unique_hkey hkey2{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        test_iterator = wil::reg::key_bstr_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::key_bstr_iterator{});
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_KeyName1));
        test_iterator_copy = test_iterator;
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_KeyName1));
        test_iterator += 1;
        REQUIRE(test_iterator == wil::reg::key_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_bstr_iterator{});
    }

    SECTION("key_bstr_iterator changing the iterator data string sizes to force resize and trim")
    {
        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[0]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[1]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[2]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[3]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[4]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[5]);

        const auto begin = wil::reg::key_bstr_iterator{write_hkey.get()};
        const auto end = wil::reg::key_bstr_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](const auto& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](auto&& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty
    }

    SECTION("key_bstr_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2 - 1;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length;

        wil::unique_bstr half_length_name{SysAllocStringLen(nullptr, half_length)};
        REQUIRE(half_length_name);
        wil::unique_bstr full_length_name{SysAllocStringLen(nullptr, full_length)};
        REQUIRE(full_length_name);
        memset(half_length_name.get(), L'a', half_length * sizeof(wchar_t));
        memset(full_length_name.get(), L'b', full_length * sizeof(wchar_t));

        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::create_unique_key(write_hkey.get(), half_length_name.get());
        wil::reg::create_unique_key(write_hkey.get(), full_length_name.get());

        auto iterate_keys = wil::reg::key_bstr_iterator{write_hkey.get()};
        const auto& half_length_key = *iterate_keys;
        REQUIRE(half_length == SysStringLen(half_length_key.name.get()));
        REQUIRE(0 == wcscmp(half_length_key.name.get(), half_length_name.get()));
        ++iterate_keys;
        const auto& full_length_key = *iterate_keys;
        REQUIRE(full_length == SysStringLen(full_length_key.name.get()));
        REQUIRE(0 == wcscmp(full_length_key.name.get(), full_length_name.get()));
        ++iterate_keys;
        REQUIRE(iterate_keys == wil::reg::key_bstr_iterator{});
    }

    SECTION("key_bstr_iterator with many subkeys - std::for_each and std::count usage")
    {
        wil::unique_hkey enum_hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName1);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName2);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName3);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName4);

        uint32_t count = 0;
        std::for_each(wil::reg::key_bstr_iterator{enum_hkey.get()}, wil::reg::key_bstr_iterator{}, [&](const auto& key_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        });
        REQUIRE(count == 4);

        auto std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName1);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName2);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName3);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName4);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), L"xyz");
            });
        REQUIRE(std_count == 0);

#if defined(__WIL_WINREG_STL)
        // repeat with wil::shared_hkey
        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName1);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName2);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName3);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName4);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), L"xyz");
            });
        REQUIRE(std_count == 0);
#endif // #if defined(__WIL_WINREG_STL)
    }

    SECTION("key_bstr_iterator with many subkeys - range-for iterator usage")
    {
        wil::unique_hkey enum_hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName1);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName2);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName3);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName4);

        uint32_t count = 0;
        for (const auto& key_data : wil::make_range(wil::reg::key_bstr_iterator{enum_hkey.get()}, wil::reg::key_bstr_iterator{}))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
    }
}
#endif

TEST_CASE("BasicRegistryTests::value_heap_string_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("value_heap_string_iterator with no values")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};

        auto test_iterator{wil::reg::value_heap_string_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::value_heap_string_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
    }

    SECTION("value_heap_string_iterator with one value - manual iterator usage")
    {
        wil::unique_hkey write_hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(write_hkey.get(), test_enum_valueName1, 0);

        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};

        // both ways to access the iterator data
        auto test_iterator = wil::reg::value_heap_string_iterator(hkey.get());
        REQUIRE(test_iterator == wil::reg::value_heap_string_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        ++test_iterator;
        REQUIRE(test_iterator == wil::reg::value_heap_string_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_iterator{});

        test_iterator = wil::reg::value_heap_string_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::value_heap_string_iterator{});
        REQUIRE(test_iterator == wil::reg::value_heap_string_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        ++test_iterator;
        REQUIRE(test_iterator == wil::reg::value_heap_string_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_iterator{});

        const wil::unique_hkey hkey2{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        test_iterator = wil::reg::value_heap_string_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::value_heap_string_iterator{});
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        test_iterator_copy = test_iterator;
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        test_iterator += 1;
        REQUIRE(test_iterator == wil::reg::value_heap_string_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_iterator{});
    }

    SECTION("value_heap_string_iterator changing the iterator data string sizes to force resize and trim")
    {
        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[0], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[1], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[2], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[3], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[4], L"");
        wil::reg::set_value_string(write_hkey.get(), enumTestNames[5], L"");

        const auto begin = wil::reg::value_heap_string_iterator{write_hkey.get()};
        const auto end = wil::reg::value_heap_string_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](const auto& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](auto&& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));
    }

    SECTION("value_heap_string_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2 - 2;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length;

        auto half_length_name{::wil::make_unique_string_nothrow<::wil::unique_process_heap_string>(nullptr, half_length)};
        REQUIRE(half_length_name.get());
        auto full_length_name{::wil::make_unique_string_nothrow<::wil::unique_process_heap_string>(nullptr, full_length)};
        REQUIRE(full_length_name);
        memset(half_length_name.get(), L'a', half_length * sizeof(wchar_t));
        memset(full_length_name.get(), L'b', full_length * sizeof(wchar_t));

        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::set_value_string(write_hkey.get(), half_length_name.get(), half_length_name.get());
        wil::reg::set_value_string(write_hkey.get(), full_length_name.get(), full_length_name.get());

        auto iterate_values = wil::reg::value_heap_string_iterator{write_hkey.get()};
        const auto& half_length_value = *iterate_values;
        REQUIRE(half_length == wcslen(half_length_value.name.get()));
        REQUIRE(0 == wcscmp(half_length_value.name.get(), half_length_name.get()));
        ++iterate_values;
        const auto& full_length_value = *iterate_values;
        REQUIRE(full_length == wcslen(full_length_value.name.get()));
        REQUIRE(0 == wcscmp(full_length_value.name.get(), full_length_name.get()));
        ++iterate_values;
        REQUIRE(iterate_values == wil::reg::value_heap_string_iterator{});
    }

    SECTION("value_heap_string_iterator with many values - std::for_each usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(hkey.get(), test_enum_valueName1, 0);
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1ul);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ull);
        wil::reg::set_value(hkey.get(), test_enum_valueName4, L"four");

        uint32_t count = 0;
        std::for_each(wil::reg::value_heap_string_iterator(hkey.get()), wil::reg::value_heap_string_iterator{}, [&](const auto& value_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        });
        REQUIRE(count == 4);

        count = 0;
        const auto testIterator = wil::reg::value_heap_string_iterator(hkey.get());
        const auto testEndIterator = wil::reg::value_heap_string_iterator{};
        std::for_each(testIterator, testEndIterator, [&](const auto& value_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        });
        REQUIRE(count == 4);

        auto std_count = std::count_if(
            wil::reg::value_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_heap_string_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), test_enum_valueName1)) && (value_data.type == REG_DWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_heap_string_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), test_enum_valueName2)) && (value_data.type == REG_DWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_heap_string_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), test_enum_valueName3)) && (value_data.type == REG_QWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_heap_string_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), test_enum_valueName4)) && (value_data.type == REG_SZ);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_heap_string_iterator{},
            [&](const auto& value_data) {
                return (0 == wcscmp(value_data.name.get(), L"xyz"));
            });
        REQUIRE(std_count == 0);
    }

    SECTION("value_heap_string_iterator with many values - range-for iterator usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(hkey.get(), test_enum_valueName1, 0);
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1ul);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ull);
        wil::reg::set_value(hkey.get(), test_enum_valueName4, L"four");

        uint32_t count = 0;
        for (const auto& value_data :
             wil::make_range(wil::reg::value_heap_string_iterator{hkey.get()}, wil::reg::value_heap_string_iterator{}))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }

        count = 0;
        const auto testIterator = wil::reg::value_heap_string_iterator(hkey.get());
        const auto testEndIterator = wil::reg::value_heap_string_iterator{};
        for (const auto& value_data : wil::make_range(testIterator, testEndIterator))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE(count == 4);
    }
}

TEST_CASE("BasicRegistryTests::key_heap_string_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("key_heap_string_iterator with no subkeys")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};

        auto test_iterator{wil::reg::key_heap_string_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::key_heap_string_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
    }

    SECTION("key_heap_string_iterator with one subkey - manual iterator usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::create_unique_key(hkey.get(), test_enum_KeyName1);

        const auto key_enum = wil::reg::key_heap_string_iterator(hkey.get());
        const auto key_end = wil::reg::key_heap_string_iterator{};
        REQUIRE(key_enum != key_end);

        auto test_iterator = wil::reg::key_heap_string_iterator(hkey.get());
        REQUIRE(test_iterator == wil::reg::key_heap_string_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_KeyName1));
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_heap_string_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_KeyName1));
        ++test_iterator;
        REQUIRE(test_iterator == key_end);
        REQUIRE(test_iterator == wil::reg::key_heap_string_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == key_end);
        REQUIRE(test_iterator_copy == wil::reg::key_heap_string_iterator{});

        test_iterator = wil::reg::key_heap_string_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::key_heap_string_iterator{});
        REQUIRE(test_iterator == wil::reg::key_heap_string_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_KeyName1));
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_heap_string_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_KeyName1));
        ++test_iterator;
        REQUIRE(test_iterator == key_end);
        REQUIRE(test_iterator == wil::reg::key_heap_string_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == key_end);
        REQUIRE(test_iterator_copy == wil::reg::key_heap_string_iterator{});

        const wil::unique_hkey hkey2{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        test_iterator = wil::reg::key_heap_string_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::key_heap_string_iterator{});
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_KeyName1));
        test_iterator_copy = test_iterator;
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_KeyName1));
        test_iterator += 1;
        REQUIRE(test_iterator == wil::reg::key_heap_string_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_heap_string_iterator{});
    }

    SECTION("key_heap_string_iterator changing the iterator data string sizes to force resize and trim")
    {
        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[0]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[1]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[2]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[3]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[4]);
        wil::reg::create_unique_key(write_hkey.get(), enumTestNames[5]);

        const auto begin = wil::reg::key_heap_string_iterator{write_hkey.get()};
        const auto end = wil::reg::key_heap_string_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](const auto& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](auto&& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty
    }

    SECTION("key_heap_string_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2 - 2;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length;

        auto half_length_name{::wil::make_unique_string_nothrow<::wil::unique_process_heap_string>(nullptr, half_length)};
        REQUIRE(half_length_name.get());
        auto full_length_name{::wil::make_unique_string_nothrow<::wil::unique_process_heap_string>(nullptr, full_length)};
        REQUIRE(full_length_name);
        memset(half_length_name.get(), L'a', half_length * sizeof(wchar_t));
        memset(full_length_name.get(), L'b', full_length * sizeof(wchar_t));

        const auto write_hkey = wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite);
        wil::reg::create_unique_key(write_hkey.get(), half_length_name.get());
        wil::reg::create_unique_key(write_hkey.get(), full_length_name.get());

        auto iterate_keys = wil::reg::key_heap_string_iterator{write_hkey.get()};
        const auto& half_length_key = *iterate_keys;
        REQUIRE(half_length == wcslen(half_length_key.name.get()));
        REQUIRE(0 == wcscmp(half_length_key.name.get(), half_length_name.get()));
        ++iterate_keys;
        const auto& full_length_key = *iterate_keys;
        REQUIRE(full_length == wcslen(full_length_key.name.get()));
        REQUIRE(0 == wcscmp(full_length_key.name.get(), full_length_name.get()));
        ++iterate_keys;
        REQUIRE(iterate_keys == wil::reg::key_heap_string_iterator{});
    }

    SECTION("key_heap_string_iterator with many subkeys - std::for_each and std::count usage")
    {
        wil::unique_hkey enum_hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName1);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName2);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName3);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName4);

        uint32_t count = 0;
        std::for_each(wil::reg::key_heap_string_iterator{enum_hkey.get()}, wil::reg::key_heap_string_iterator{}, [&](const auto& key_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        });
        REQUIRE(count == 4);

        auto std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName1);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName2);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName3);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName4);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), L"xyz");
            });
        REQUIRE(std_count == 0);

#if defined(__WIL_WINREG_STL)
        // repeat with wil::shared_hkey
        std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName1);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName2);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName3);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), test_enum_KeyName4);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_heap_string_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_heap_string_iterator{},
            [](const auto& test_data) {
                return 0 == wcscmp(test_data.name.get(), L"xyz");
            });
        REQUIRE(std_count == 0);
#endif // #if defined(__WIL_WINREG_STL)
    }

    SECTION("key_heap_string_iterator with many subkeys - range-for iterator usage")
    {
        wil::unique_hkey enum_hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName1);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName2);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName3);
        wil::reg::create_unique_key(enum_hkey.get(), test_enum_KeyName4);

        uint32_t count = 0;
        for (const auto& key_data :
             wil::make_range(wil::reg::key_heap_string_iterator{enum_hkey.get()}, wil::reg::key_heap_string_iterator{}))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
    }
}
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_OLEAUTO_H_)
TEST_CASE("BasicRegistryTests::value_bstr_nothrow_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("value_bstr_nothrow_iterator with no values")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey));

        auto test_iterator{wil::reg::value_bstr_nothrow_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::value_bstr_nothrow_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);
        REQUIRE(test_iterator.at_end());
        REQUIRE(test_end_iterator.at_end());

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
        REQUIRE(iterator_copy.at_end());
    }

    SECTION("value_bstr_nothrow_iterator with one value - manual iterator usage")
    {
        wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(write_hkey.get(), test_enum_valueName1, 0));

        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey));

        // both ways to access the iterator data
        auto test_iterator = wil::reg::value_bstr_nothrow_iterator(hkey.get());
        REQUIRE(test_iterator == wil::reg::value_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        REQUIRE_SUCCEEDED(test_iterator.move_next());
        REQUIRE_SUCCEEDED(test_iterator.last_error());
        REQUIRE(test_iterator.at_end());
        REQUIRE(test_iterator == wil::reg::value_bstr_nothrow_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_nothrow_iterator{});
        REQUIRE(test_iterator_copy.at_end());

        test_iterator = wil::reg::value_bstr_nothrow_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::value_bstr_nothrow_iterator{});
        REQUIRE(test_iterator == wil::reg::value_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        REQUIRE(!test_iterator.at_end());
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        REQUIRE(!test_iterator_copy.at_end());
        REQUIRE_SUCCEEDED(test_iterator.move_next());
        REQUIRE_SUCCEEDED(test_iterator.last_error());
        REQUIRE(test_iterator.at_end());
        REQUIRE(test_iterator == wil::reg::value_bstr_nothrow_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_nothrow_iterator{});

        wil::unique_hkey hkey2;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey2));
        test_iterator = wil::reg::value_bstr_nothrow_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::value_bstr_nothrow_iterator{});
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        REQUIRE(!test_iterator.at_end());
        test_iterator_copy = test_iterator;
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        REQUIRE(!test_iterator_copy.at_end());
        REQUIRE_SUCCEEDED(test_iterator.move_next());
        REQUIRE_SUCCEEDED(test_iterator.last_error());
        REQUIRE(test_iterator.at_end());
        REQUIRE(test_iterator == wil::reg::value_bstr_nothrow_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_nothrow_iterator{});
        REQUIRE(test_iterator_copy.at_end());
    }

    SECTION("value_bstr_nothrow_iterator changing the iterator data string sizes to force resize and trim")
    {
        wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[0], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[1], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[2], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[3], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[4], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[5], L""));

        const auto begin = wil::reg::value_bstr_nothrow_iterator{write_hkey.get()};
        const auto end = wil::reg::value_bstr_nothrow_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](const auto& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](auto&& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames));
    }

    SECTION("value_bstr_nothrow_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2 - 2;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length;

        wil::unique_bstr half_length_name{SysAllocStringLen(nullptr, half_length)};
        REQUIRE(half_length_name);
        wil::unique_bstr full_length_name{SysAllocStringLen(nullptr, full_length)};
        REQUIRE(full_length_name);
        memset(half_length_name.get(), L'a', half_length * sizeof(wchar_t));
        memset(full_length_name.get(), L'b', full_length * sizeof(wchar_t));

        ::wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), half_length_name.get(), half_length_name.get()));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), full_length_name.get(), full_length_name.get()));

        auto iterate_values = wil::reg::value_bstr_nothrow_iterator{write_hkey.get()};
        REQUIRE_SUCCEEDED(iterate_values.last_error());
        const auto& half_length_value = *iterate_values;
        REQUIRE(half_length == SysStringLen(half_length_value.name.get()));
        REQUIRE(0 == wcscmp(half_length_value.name.get(), half_length_name.get()));
        ++iterate_values;
        REQUIRE_SUCCEEDED(iterate_values.last_error());
        const auto& full_length_value = *iterate_values;
        REQUIRE(full_length == SysStringLen(full_length_value.name.get()));
        REQUIRE(0 == wcscmp(full_length_value.name.get(), full_length_name.get()));
        ++iterate_values;
        REQUIRE(iterate_values == wil::reg::value_bstr_nothrow_iterator{});
    }

    SECTION("value_bstr_nothrow_iterator with many values - range-for iterator usage")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName1, 0));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName2, 1ul));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName3, 3ull));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName4, L"four"));

        uint32_t count = 0;
        for (const auto& value_data :
             wil::make_range(wil::reg::value_bstr_nothrow_iterator{hkey.get()}, wil::reg::value_bstr_nothrow_iterator{}))
        {
            if (value_data.at_end())
            {
                break;
            }

            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE(count == 4);

        count = 0;
        const auto testIterator = wil::reg::value_bstr_nothrow_iterator(hkey.get());
        const auto testEndIterator = wil::reg::value_bstr_nothrow_iterator{};
        for (const auto& value_data : wil::make_range(testIterator, testEndIterator))
        {
            if (value_data.at_end())
            {
                break;
            }

            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE_SUCCEEDED(testIterator.last_error());
        REQUIRE(count == 4);

        count = 0;
        auto manual_iterator = wil::reg::value_bstr_nothrow_iterator{hkey.get()};
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        while (!manual_iterator.at_end())
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName1));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName2));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName3));
                REQUIRE(manual_iterator->type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName4));
                REQUIRE(manual_iterator->type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }

            const auto hr = manual_iterator.move_next();
            REQUIRE_SUCCEEDED(hr);
            REQUIRE_SUCCEEDED(manual_iterator.last_error());
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(manual_iterator->at_end());
        REQUIRE(count == 4);

        count = 0;
        manual_iterator = wil::reg::value_bstr_nothrow_iterator{hkey.get()};
        for (; !manual_iterator.at_end(); manual_iterator.move_next())
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName1));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName2));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName3));
                REQUIRE(manual_iterator->type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName4));
                REQUIRE(manual_iterator->type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(manual_iterator->at_end());
        REQUIRE(count == 4);
    }
}

TEST_CASE("BasicRegistryTests::key_bstr_nothrow_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("key_bstr_nothrow_iterator with no subkeys")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey));

        auto test_iterator{wil::reg::key_bstr_nothrow_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::key_bstr_nothrow_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
    }

    SECTION("key_bstr_nothrow_iterator with one subkey - manual iterator usage")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(hkey.get(), test_enum_KeyName1, subkey));
        subkey.reset();

        const auto key_enum = wil::reg::key_bstr_nothrow_iterator(hkey.get());
        const auto key_end = wil::reg::key_bstr_nothrow_iterator{};
        REQUIRE(key_enum != key_end);

        auto key_iterator = wil::reg::key_bstr_nothrow_iterator(hkey.get());
        REQUIRE(key_iterator == wil::reg::key_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*key_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator->name.get(), test_enum_KeyName1));
        auto key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == wil::reg::key_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*key_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator_copy->name.get(), test_enum_KeyName1));
        REQUIRE_SUCCEEDED(key_iterator.move_next());
        REQUIRE_SUCCEEDED(key_iterator.last_error());
        REQUIRE(key_iterator == key_end);
        REQUIRE(key_iterator == wil::reg::key_bstr_nothrow_iterator{});
        key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == key_end);
        REQUIRE(key_iterator_copy == wil::reg::key_bstr_nothrow_iterator{});

        key_iterator = wil::reg::key_bstr_nothrow_iterator(hkey.get());
        REQUIRE(key_iterator != wil::reg::key_bstr_nothrow_iterator{});
        REQUIRE(key_iterator == wil::reg::key_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*key_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator->name.get(), test_enum_KeyName1));
        key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == wil::reg::key_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*key_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator_copy->name.get(), test_enum_KeyName1));
        REQUIRE_SUCCEEDED(key_iterator.move_next());
        REQUIRE_SUCCEEDED(key_iterator.last_error());
        REQUIRE(key_iterator == key_end);
        REQUIRE(key_iterator == wil::reg::key_bstr_nothrow_iterator{});
        key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == key_end);
        REQUIRE(key_iterator_copy == wil::reg::key_bstr_nothrow_iterator{});

        wil::unique_hkey hkey2;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey2));
        key_iterator = wil::reg::key_bstr_nothrow_iterator(hkey2.get());
        REQUIRE(key_iterator != wil::reg::key_bstr_nothrow_iterator{});
        REQUIRE(0 == wcscmp((*key_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator->name.get(), test_enum_KeyName1));
        key_iterator_copy = key_iterator;
        REQUIRE(0 == wcscmp((*key_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator_copy->name.get(), test_enum_KeyName1));
        REQUIRE_SUCCEEDED(key_iterator.move_next());
        REQUIRE_SUCCEEDED(key_iterator.last_error());
        REQUIRE(key_iterator == wil::reg::key_bstr_nothrow_iterator{});
        key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == wil::reg::key_bstr_nothrow_iterator{});
    }

    SECTION("key_bstr_nothrow_iterator changing the iterator data string sizes to force resize and trim")
    {
        wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[0], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[1], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[2], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[3], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[4], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[5], subkey));

        const auto begin = wil::reg::key_bstr_nothrow_iterator{write_hkey.get()};
        const auto end = wil::reg::key_bstr_nothrow_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](const auto& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](auto&& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty
    }

    SECTION("key_bstr_nothrow_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2 - 1;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length;

        wil::unique_bstr half_length_name{SysAllocStringLen(nullptr, half_length)};
        REQUIRE(half_length_name);
        wil::unique_bstr full_length_name{SysAllocStringLen(nullptr, full_length)};
        REQUIRE(full_length_name);
        memset(half_length_name.get(), L'a', half_length * sizeof(wchar_t));
        memset(full_length_name.get(), L'b', full_length * sizeof(wchar_t));

        wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), half_length_name.get(), subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), full_length_name.get(), subkey));

        auto iterate_keys = wil::reg::key_bstr_nothrow_iterator{write_hkey.get()};
        REQUIRE_SUCCEEDED(iterate_keys.last_error());
        const auto& half_length_key = *iterate_keys;
        REQUIRE(half_length == SysStringLen(half_length_key.name.get()));
        REQUIRE(0 == wcscmp(half_length_key.name.get(), half_length_name.get()));
        ++iterate_keys;
        REQUIRE_SUCCEEDED(iterate_keys.last_error());
        const auto& full_length_key = *iterate_keys;
        REQUIRE(full_length == SysStringLen(full_length_key.name.get()));
        REQUIRE(0 == wcscmp(full_length_key.name.get(), full_length_name.get()));
        ++iterate_keys;
        REQUIRE(iterate_keys == wil::reg::key_bstr_nothrow_iterator{});
    }

    SECTION("key_bstr_nothrow_iterator with many subkeys - range-for iterator usage")
    {
        wil::unique_hkey enum_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, enum_hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(enum_hkey.get(), test_enum_KeyName1, subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(enum_hkey.get(), test_enum_KeyName2, subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(enum_hkey.get(), test_enum_KeyName3, subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(enum_hkey.get(), test_enum_KeyName4, subkey));
        subkey.reset();

        uint32_t count = 0;
        for (const auto& key_data :
             wil::make_range(wil::reg::key_bstr_nothrow_iterator{enum_hkey.get()}, wil::reg::key_bstr_nothrow_iterator{}))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE(count == 4);

        count = 0;
        const auto testIterator = wil::reg::key_bstr_nothrow_iterator(enum_hkey.get());
        const auto testEndIterator = wil::reg::key_bstr_nothrow_iterator{};
        for (const auto& key_data : wil::make_range(testIterator, testEndIterator))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE_SUCCEEDED(testIterator.last_error());
        REQUIRE(count == 4);

        count = 0;
        auto manual_iterator = wil::reg::key_bstr_nothrow_iterator{enum_hkey.get()};
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        while (!manual_iterator.at_end())
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }

            const auto hr = manual_iterator.move_next();
            REQUIRE_SUCCEEDED(hr);
            REQUIRE_SUCCEEDED(manual_iterator.last_error());
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(manual_iterator.at_end());
        REQUIRE(count == 4);

        count = 0;
        manual_iterator = wil::reg::key_bstr_nothrow_iterator{enum_hkey.get()};
        for (; !manual_iterator.at_end(); manual_iterator.move_next())
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(count == 4);
    }
}
#endif
TEST_CASE("BasicRegistryTests::value_heap_string_nothrow_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("value_heap_string_nothrow_iterator with no values")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey));

        auto test_iterator{wil::reg::value_heap_string_nothrow_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::value_heap_string_nothrow_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);
        REQUIRE(test_iterator.at_end());
        REQUIRE(test_end_iterator.at_end());

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
        REQUIRE(iterator_copy.at_end());
    }

    SECTION("value_heap_string_nothrow_iterator with one value - manual iterator usage")
    {
        wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(write_hkey.get(), test_enum_valueName1, 0));

        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey));

        // both ways to access the iterator data
        auto test_iterator = wil::reg::value_heap_string_nothrow_iterator(hkey.get());
        REQUIRE(test_iterator == wil::reg::value_heap_string_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        REQUIRE_SUCCEEDED(test_iterator.move_next());
        REQUIRE_SUCCEEDED(test_iterator.last_error());
        REQUIRE(test_iterator.at_end());
        REQUIRE(test_iterator == wil::reg::value_heap_string_nothrow_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_nothrow_iterator{});
        REQUIRE(test_iterator_copy.at_end());

        test_iterator = wil::reg::value_heap_string_nothrow_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::value_heap_string_nothrow_iterator{});
        REQUIRE(test_iterator == wil::reg::value_heap_string_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        REQUIRE(!test_iterator.at_end());
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        REQUIRE(!test_iterator_copy.at_end());
        REQUIRE_SUCCEEDED(test_iterator.move_next());
        REQUIRE_SUCCEEDED(test_iterator.last_error());
        REQUIRE(test_iterator.at_end());
        REQUIRE(test_iterator == wil::reg::value_heap_string_nothrow_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_nothrow_iterator{});

        wil::unique_hkey hkey2;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey2));
        test_iterator = wil::reg::value_heap_string_nothrow_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::value_heap_string_nothrow_iterator{});
        REQUIRE(0 == wcscmp((*test_iterator).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator->name.get(), test_enum_valueName1));
        REQUIRE(!test_iterator.at_end());
        test_iterator_copy = test_iterator;
        REQUIRE(0 == wcscmp((*test_iterator_copy).name.get(), test_enum_valueName1));
        REQUIRE(0 == wcscmp(test_iterator_copy->name.get(), test_enum_valueName1));
        REQUIRE(!test_iterator_copy.at_end());
        REQUIRE_SUCCEEDED(test_iterator.move_next());
        REQUIRE_SUCCEEDED(test_iterator.last_error());
        REQUIRE(test_iterator.at_end());
        REQUIRE(test_iterator == wil::reg::value_heap_string_nothrow_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_heap_string_nothrow_iterator{});
        REQUIRE(test_iterator_copy.at_end());
    }

    SECTION("value_heap_string_nothrow_iterator changing the iterator data string sizes to force resize and trim")
    {
        wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[0], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[1], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[2], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[3], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[4], L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), enumTestNames[5], L""));

        const auto begin = wil::reg::value_heap_string_nothrow_iterator{write_hkey.get()};
        const auto end = wil::reg::value_heap_string_nothrow_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](const auto& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](auto&& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(nameAndType.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames));
    }

    SECTION("value_heap_string_nothrow_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2 - 2;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_valuename_length;

        auto half_length_name{::wil::make_unique_string_nothrow<::wil::unique_process_heap_string>(nullptr, half_length)};
        REQUIRE(half_length_name.get());
        auto full_length_name{::wil::make_unique_string_nothrow<::wil::unique_process_heap_string>(nullptr, full_length)};
        REQUIRE(full_length_name);
        memset(half_length_name.get(), L'a', half_length * sizeof(wchar_t));
        memset(full_length_name.get(), L'b', full_length * sizeof(wchar_t));

        wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), half_length_name.get(), L""));
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(write_hkey.get(), full_length_name.get(), L""));

        auto iterate_values = wil::reg::value_heap_string_nothrow_iterator{write_hkey.get()};
        REQUIRE_SUCCEEDED(iterate_values.last_error());
        const auto& half_length_value = *iterate_values;
        REQUIRE(half_length == wcslen(half_length_value.name.get()));
        REQUIRE(0 == wcscmp(half_length_value.name.get(), half_length_name.get()));
        ++iterate_values;
        REQUIRE_SUCCEEDED(iterate_values.last_error());
        const auto& full_length_value = *iterate_values;
        REQUIRE(full_length == wcslen(full_length_value.name.get()));
        REQUIRE(0 == wcscmp(full_length_value.name.get(), full_length_name.get()));
        ++iterate_values;
        REQUIRE(iterate_values == wil::reg::value_heap_string_nothrow_iterator{});
    }

    SECTION("value_heap_string_nothrow_iterator with many values - range-for iterator usage")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName1, 0));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName2, 1ul));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName3, 3ull));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName4, L"four"));

        uint32_t count = 0;
        for (const auto& value_data :
             wil::make_range(wil::reg::value_heap_string_nothrow_iterator{hkey.get()}, wil::reg::value_heap_string_nothrow_iterator{}))
        {
            if (value_data.at_end())
            {
                break;
            }

            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE(count == 4);

        count = 0;
        const auto testIterator = wil::reg::value_heap_string_nothrow_iterator(hkey.get());
        const auto testEndIterator = wil::reg::value_heap_string_nothrow_iterator{};
        for (const auto& value_data : wil::make_range(testIterator, testEndIterator))
        {
            if (value_data.at_end())
            {
                break;
            }

            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(value_data.name.get(), test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE_SUCCEEDED(testIterator.last_error());
        REQUIRE(count == 4);

        count = 0;
        auto manual_iterator = wil::reg::value_heap_string_nothrow_iterator{hkey.get()};
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        while (!manual_iterator.at_end())
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName1));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName2));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName3));
                REQUIRE(manual_iterator->type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName4));
                REQUIRE(manual_iterator->type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }

            const auto hr = manual_iterator.move_next();
            REQUIRE_SUCCEEDED(hr);
            REQUIRE_SUCCEEDED(manual_iterator.last_error());
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(manual_iterator->at_end());
        REQUIRE(count == 4);

        count = 0;
        manual_iterator = wil::reg::value_heap_string_nothrow_iterator{hkey.get()};
        for (; !manual_iterator.at_end(); manual_iterator.move_next())
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName1));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 2:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName2));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 3:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName3));
                REQUIRE(manual_iterator->type == REG_QWORD);
                break;
            case 4:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_valueName4));
                REQUIRE(manual_iterator->type == REG_SZ);
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(manual_iterator->at_end());
        REQUIRE(count == 4);
    }
}

TEST_CASE("BasicRegistryTests::key_heap_string_nothrow_iterator", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("key_heap_string_nothrow_iterator with no subkeys")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey));

        auto test_iterator{wil::reg::key_heap_string_nothrow_iterator(hkey.get())};
        auto test_end_iterator{wil::reg::key_heap_string_nothrow_iterator{}};
        REQUIRE(test_iterator == test_end_iterator);

        const auto iterator_copy = test_iterator;
        REQUIRE(iterator_copy == test_iterator);
        REQUIRE(iterator_copy == test_end_iterator);
    }

    SECTION("key_heap_string_nothrow_iterator with one subkey - manual iterator usage")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(hkey.get(), test_enum_KeyName1, subkey));
        subkey.reset();

        const auto key_enum = wil::reg::key_heap_string_nothrow_iterator(hkey.get());
        const auto key_end = wil::reg::key_heap_string_nothrow_iterator{};
        REQUIRE(key_enum != key_end);

        auto key_iterator = wil::reg::key_heap_string_nothrow_iterator(hkey.get());
        REQUIRE(key_iterator == wil::reg::key_heap_string_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*key_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator->name.get(), test_enum_KeyName1));
        auto key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == wil::reg::key_heap_string_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*key_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator_copy->name.get(), test_enum_KeyName1));
        REQUIRE_SUCCEEDED(key_iterator.move_next());
        REQUIRE_SUCCEEDED(key_iterator.last_error());
        REQUIRE(key_iterator == key_end);
        REQUIRE(key_iterator == wil::reg::key_heap_string_nothrow_iterator{});
        key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == key_end);
        REQUIRE(key_iterator_copy == wil::reg::key_heap_string_nothrow_iterator{});

        key_iterator = wil::reg::key_heap_string_nothrow_iterator(hkey.get());
        REQUIRE(key_iterator != wil::reg::key_heap_string_nothrow_iterator{});
        REQUIRE(key_iterator == wil::reg::key_heap_string_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*key_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator->name.get(), test_enum_KeyName1));
        key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == wil::reg::key_heap_string_nothrow_iterator(hkey.get()));
        REQUIRE(0 == wcscmp((*key_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator_copy->name.get(), test_enum_KeyName1));
        REQUIRE_SUCCEEDED(key_iterator.move_next());
        REQUIRE_SUCCEEDED(key_iterator.last_error());
        REQUIRE(key_iterator == key_end);
        REQUIRE(key_iterator == wil::reg::key_heap_string_nothrow_iterator{});
        key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == key_end);
        REQUIRE(key_iterator_copy == wil::reg::key_heap_string_nothrow_iterator{});

        wil::unique_hkey hkey2;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey2));
        key_iterator = wil::reg::key_heap_string_nothrow_iterator(hkey2.get());
        REQUIRE(key_iterator != wil::reg::key_heap_string_nothrow_iterator{});
        REQUIRE(0 == wcscmp((*key_iterator).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator->name.get(), test_enum_KeyName1));
        key_iterator_copy = key_iterator;
        REQUIRE(0 == wcscmp((*key_iterator_copy).name.get(), test_enum_KeyName1));
        REQUIRE(0 == wcscmp(key_iterator_copy->name.get(), test_enum_KeyName1));
        REQUIRE_SUCCEEDED(key_iterator.move_next());
        REQUIRE_SUCCEEDED(key_iterator.last_error());
        REQUIRE(key_iterator == wil::reg::key_heap_string_nothrow_iterator{});
        key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == wil::reg::key_heap_string_nothrow_iterator{});
    }

    SECTION("key_heap_string_nothrow_iterator changing the iterator data string sizes to force resize and trim")
    {
        wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[0], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[1], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[2], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[3], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[4], subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), enumTestNames[5], subkey));

        const auto begin = wil::reg::key_heap_string_nothrow_iterator{write_hkey.get()};
        const auto end = wil::reg::key_heap_string_nothrow_iterator{};

        size_t count = 0u;
        std::for_each(begin, end, [&](auto keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](const auto& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](auto&& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(0 == wcscmp(keyInfo.name.get(), enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty
    }

    SECTION("key_heap_string_nothrow_iterator max name lengths")
    {
        const auto half_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2 - 2;
        const auto full_length = ::wil::reg::reg_iterator_details::iterator_max_keyname_length;

        auto half_length_name{::wil::make_unique_string_nothrow<::wil::unique_process_heap_string>(nullptr, half_length)};
        REQUIRE(half_length_name.get());
        auto full_length_name{::wil::make_unique_string_nothrow<::wil::unique_process_heap_string>(nullptr, full_length)};
        REQUIRE(full_length_name);
        memset(half_length_name.get(), L'a', half_length * sizeof(wchar_t));
        memset(full_length_name.get(), L'b', full_length * sizeof(wchar_t));

        wil::unique_hkey write_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), half_length_name.get(), subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(write_hkey.get(), full_length_name.get(), subkey));

        auto iterate_values = wil::reg::key_heap_string_nothrow_iterator{write_hkey.get()};
        REQUIRE_SUCCEEDED(iterate_values.last_error());
        const auto& half_length_value = *iterate_values;
        REQUIRE(half_length == wcslen(half_length_value.name.get()));
        REQUIRE(0 == wcscmp(half_length_value.name.get(), half_length_name.get()));
        ++iterate_values;
        REQUIRE_SUCCEEDED(iterate_values.last_error());
        const auto& full_length_value = *iterate_values;
        REQUIRE(full_length == wcslen(full_length_value.name.get()));
        REQUIRE(0 == wcscmp(full_length_value.name.get(), full_length_name.get()));
        ++iterate_values;
        REQUIRE(iterate_values == wil::reg::key_heap_string_nothrow_iterator{});
    }

    SECTION("key_heap_string_nothrow_iterator with many subkeys - range-for iterator usage")
    {
        wil::unique_hkey enum_hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, enum_hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(enum_hkey.get(), test_enum_KeyName1, subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(enum_hkey.get(), test_enum_KeyName2, subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(enum_hkey.get(), test_enum_KeyName3, subkey));
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(enum_hkey.get(), test_enum_KeyName4, subkey));
        subkey.reset();

        uint32_t count = 0;
        for (const auto& key_data : wil::make_range(
                 wil::reg::key_heap_string_nothrow_iterator{enum_hkey.get()}, wil::reg::key_heap_string_nothrow_iterator{}))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE(count == 4);

        count = 0;
        const auto testIterator = wil::reg::key_heap_string_nothrow_iterator(enum_hkey.get());
        const auto testEndIterator = wil::reg::key_heap_string_nothrow_iterator{};
        for (const auto& key_data : wil::make_range(testIterator, testEndIterator))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(key_data.name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE_SUCCEEDED(testIterator.last_error());
        REQUIRE(count == 4);

        count = 0;
        auto manual_iterator = wil::reg::key_heap_string_nothrow_iterator{enum_hkey.get()};
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        while (!manual_iterator.at_end())
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }

            const auto hr = manual_iterator.move_next();
            REQUIRE_SUCCEEDED(hr);
            REQUIRE_SUCCEEDED(manual_iterator.last_error());
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(manual_iterator.at_end());
        REQUIRE(count == 4);

        count = 0;
        manual_iterator = wil::reg::key_heap_string_nothrow_iterator{enum_hkey.get()};
        for (; !manual_iterator.at_end(); manual_iterator.move_next())
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(0 == wcscmp(manual_iterator->name.get(), test_enum_KeyName4));
                break;
            default:
                REQUIRE_FAILED(false);
            }
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(count == 4);
    }
}
