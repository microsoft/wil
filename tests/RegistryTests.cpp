#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <array>

#include <windows.h>

#include <wil/filesystem.h>
#include <wil/registry.h>
#include <wil/resource.h>

#include "common.h"

constexpr auto* testSubkey = L"Software\\Microsoft\\BasicRegistryTest";
constexpr auto* dwordValueName = L"MyDwordValue";
constexpr auto* qwordValueName = L"MyQwordvalue";
constexpr auto* stringValueName = L"MyStringValue";

constexpr uint32_t test_dword_two = 2ul;
constexpr uint32_t test_dword_three = 3ul;
constexpr uint32_t test_dword_zero = 0ul;
constexpr uint64_t test_qword_zero = 0ull;

constexpr uint32_t test_expanded_string_buffer_size = 100;

constexpr std::array<DWORD, 3> dwordTestArray = { static_cast<DWORD>(-1), 1, 0 };
constexpr DWORD64 qwordTestArray[] = { static_cast<DWORD64>(-1), 1, 0 };
const std::wstring stringTestArray[] = { L".", L"", L"Hello there!", L"\0" };
const std::wstring expandedStringTestArray[] = { L".", L"", L"%WINDIR%", L"\0" };
const std::vector<std::wstring> multiStringTestArray[]{
    { {} },
    { {}, {} },
    { {}, {L"."}, {}, {L"."}, {}, {} },
    { {L"Hello there!"}, {L"Hello a second time!"}, {L"Hello a third time!"} },
    { {L""}, {L""}, {L""} },
    {},
    { {L"a"} }
};

const std::vector<BYTE> emptyStringTestValue{};
const std::vector<BYTE> nonNullTerminatedString{ {'a'}, {0}, {'b'}, {0}, {'c'}, {0}, {'d'}, {0}, {'e'}, {0}, {'f'}, {0}, {'g'}, {0}, {'h'}, {0}, {'i'}, {0}, {'j'}, {0}, {'k'}, {0}, {'l'}, {0} };
const std::wstring nonNullTerminatedStringFixed{ L"abcdefghijkl" };

const std::vector<BYTE> vectorBytesTestArray[]
{
    { {0x00} },
    {},
    { {0x1}, {0x2}, {0x3}, {0x4}, {0x5}, {0x6},{0x7}, {0x8}, {0x9},{0xa}, {0xb}, {0xc}, {0xd}, {0xe}, {0xf} }
};

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
#endif #if defined(__WIL_OLEAUTO_H_STL)

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

        wil::unique_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_three);

        // fail open if the key doesn't exist
        hr = wil::reg::open_unique_key_nothrow(hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    }
    SECTION("open_unique_key_nothrow: with string key")
    {
        // create read-write, should be able to open read and open read-write
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        // write a test value
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, test_dword_two));

        wil::unique_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_three);

        // fail open if the key doesn't exist
        hr = wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
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

        wil::shared_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(hkey.get(), subSubKey, opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_three);

        // fail open if the key doesn't exist
        hr = wil::reg::open_shared_key_nothrow(hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    }
    SECTION("open_shared_key_nothrow: with string key")
    {
        // create read-write, should be able to open read and open read-write
        wil::shared_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_shared_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        // write a test value
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, test_dword_two));

        wil::shared_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_shared_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, test_dword_three));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_three);

        // fail open if the key doesn't exist
        hr = wil::reg::open_shared_key_nothrow(HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    }
#endif // #if defined(__WIL_WINREG_STL)

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("open_unique_key: with opened key")
    {
        constexpr auto* subSubKey = L"subkey";

        wil::unique_hkey hkey{ wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite) };
        // create a sub-key under this which we will try to open - but open_key will use the above hkey
        wil::unique_hkey subkey{ wil::reg::create_unique_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite) };
        // write a test value we'll try to read from later
        wil::reg::set_value_dword(subkey.get(), dwordValueName, test_dword_two);

        wil::unique_hkey read_only_key{ wil::reg::open_unique_key(hkey.get(), subSubKey, wil::reg::key_access::read) };
        DWORD result = wil::reg::get_value_dword(read_only_key.get(), dwordValueName);
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        wil::unique_hkey read_write_key{ wil::reg::open_unique_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite) };
        wil::reg::set_value_dword(read_write_key.get(), dwordValueName, test_dword_three);
        result = wil::reg::get_value_dword(read_write_key.get(), dwordValueName);
        REQUIRE(result == test_dword_three);

        // fail get* if the value doesn't exist
        try
        {
            wil::unique_hkey invalid_key{ wil::reg::open_unique_key(hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite) };
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }
    }

    SECTION("open_unique_key: with string key")
    {
        wil::unique_hkey hkey{ wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite) };
        // write a test value we'll try to read from later
        wil::reg::set_value_dword(hkey.get(), dwordValueName, test_dword_two);

        wil::unique_hkey read_only_key{ wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::read) };
        DWORD result = wil::reg::get_value_dword(read_only_key.get(), dwordValueName);
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        wil::unique_hkey read_write_key{ wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite) };
        wil::reg::set_value_dword(read_write_key.get(), dwordValueName, test_dword_three);
        result = wil::reg::get_value_dword(read_write_key.get(), dwordValueName);
        REQUIRE(result == test_dword_three);

        // fail get* if the value doesn't exist
        try
        {
            wil::unique_hkey invalid_key{ wil::reg::open_unique_key(HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite) };
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }
    }

#if defined(__WIL_WINREG_STL)
    SECTION("open_shared_key: with opened key")
    {
        constexpr auto* subSubKey = L"subkey";

        wil::shared_hkey hkey{ wil::reg::create_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite) };
        // create a sub-key under this which we will try to open - but open_key will use the above hkey
        wil::shared_hkey subkey{ wil::reg::create_shared_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite) };
        // write a test value we'll try to read from later
        wil::reg::set_value_dword(subkey.get(), dwordValueName, test_dword_two);

        wil::shared_hkey read_only_key{ wil::reg::open_shared_key(hkey.get(), subSubKey, wil::reg::key_access::read) };
        DWORD result = wil::reg::get_value_dword(read_only_key.get(), dwordValueName);
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        wil::shared_hkey read_write_key{ wil::reg::open_shared_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite) };
        wil::reg::set_value_dword(read_write_key.get(), dwordValueName, test_dword_three);
        result = wil::reg::get_value_dword(read_write_key.get(), dwordValueName);
        REQUIRE(result == test_dword_three);

        // fail get* if the value doesn't exist
        try
        {
            wil::shared_hkey invalid_key{ wil::reg::open_shared_key(hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite) };
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }
    }

    SECTION("open_shared_key: with string key")
    {
        wil::shared_hkey hkey{ wil::reg::create_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite) };
        // write a test value we'll try to read from later
        wil::reg::set_value_dword(hkey.get(), dwordValueName, test_dword_two);

        wil::shared_hkey read_only_key{ wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::read) };
        DWORD result = wil::reg::get_value_dword(read_only_key.get(), dwordValueName);
        REQUIRE(result == test_dword_two);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, test_dword_three);
        REQUIRE(hr == E_ACCESSDENIED);

        wil::shared_hkey read_write_key{ wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite) };
        wil::reg::set_value_dword(read_write_key.get(), dwordValueName, test_dword_three);
        result = wil::reg::get_value_dword(read_write_key.get(), dwordValueName);
        REQUIRE(result == test_dword_three);

        // fail get* if the value doesn't exist
        try
        {
            wil::shared_hkey invalid_key{ wil::reg::open_shared_key(HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite) };
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }
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
    template<typename T>
    struct type_identity
    {
        using type = T;
    };

    template<typename SetT, typename GetT>
    void verify_set_nothrow(
        HKEY hkey,
        PCWSTR valueName,
        GetT value,
        std::function<HRESULT (HKEY, PCWSTR, std::add_pointer_t<typename type_identity<GetT>::type>)> getFn,
        std::function<HRESULT(HKEY, PCWSTR, SetT)> setFn)
    {
        REQUIRE_SUCCEEDED(setFn(hkey, valueName, value));
        GetT result{};
        REQUIRE_SUCCEEDED(getFn(hkey, valueName, &result));
        REQUIRE(result == value);

        // and verify default value name
        REQUIRE_SUCCEEDED(setFn(hkey, nullptr, value));
        result = {};
        REQUIRE_SUCCEEDED(getFn(hkey, nullptr, &result));
        REQUIRE(result == value);
    }

    template<typename GetT, typename WrongSetT>
    void verify_wrong_type_nothrow(
        HKEY hkey,
        PCWSTR valueName,
        WrongSetT value,
        std::function<HRESULT(HKEY, PCWSTR, std::add_pointer_t<GetT>)> getFn,
        std::function<HRESULT(HKEY, PCWSTR, typename type_identity<WrongSetT>::type)> setFn)
    {
        REQUIRE_SUCCEEDED(setFn(hkey, valueName, value));
        GetT result{};
        const HRESULT hr = getFn(hkey, valueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    template<typename SetT, typename GetT, size_t N>
    void verify_nothrow(
        std::array<GetT, N> testValues,
        PCWSTR valueName,
        std::function<HRESULT(HKEY, PCWSTR, std::add_pointer_t<typename type_identity<GetT>::type>)> getFn,
        std::function<HRESULT(HKEY, PCWSTR, SetT)> setFn)
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : testValues)
        {
            verify_set_nothrow<uint32_t>(
                hkey.get(),
                valueName,
                value,
                static_cast<HRESULT(*)(HKEY, PCWSTR, DWORD*)>(wil::reg::get_value_dword_nothrow),
                static_cast<HRESULT(*)(HKEY, PCWSTR, uint32_t)>(wil::reg::set_value_dword_nothrow));
        }

        // fail get* if the value doesn't exist
        DWORD result{};
        auto hr = getFn(hkey.get(), (std::wstring(dwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        verify_wrong_type_nothrow<GetT>(
            hkey.get(),
            qwordValueName,
            test_qword_zero,
            getFn,
            static_cast<HRESULT(*)(HKEY, PCWSTR, uint64_t)>(wil::reg::set_value_qword_nothrow));
    }

    template<typename SetT, typename GetT>
    void verify_set_subkey_nothrow(
        HKEY hkey,
        PCWSTR subkey,
        PCWSTR valueName,
        GetT value,
        std::function<HRESULT(HKEY, PCWSTR, PCWSTR, std::add_pointer_t<typename type_identity<GetT>::type>)> getFn,
        std::function<HRESULT(HKEY, PCWSTR, PCWSTR, SetT)> setFn)
    {
        REQUIRE_SUCCEEDED(setFn(hkey, subkey, valueName, value));
        GetT result{};
        REQUIRE_SUCCEEDED(getFn(hkey, subkey, valueName, &result));
        REQUIRE(result == value);

        // and verify default value name
        REQUIRE_SUCCEEDED(setFn(hkey, subkey, nullptr, value));
        result = {};
        REQUIRE_SUCCEEDED(getFn(hkey, subkey, nullptr, &result));
        REQUIRE(result == value);
    }
}

TEST_CASE("BasicRegistryTests::Dwords", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_dword_nothrow/get_value_dword_nothrow: with opened key")
    {
        verify_nothrow<uint32_t, DWORD>(
            dwordTestArray,
            dwordValueName,
            static_cast<HRESULT(*)(HKEY, PCWSTR, DWORD*)>(wil::reg::get_value_dword_nothrow),
            static_cast<HRESULT(*)(HKEY, PCWSTR, uint32_t)>(wil::reg::set_value_dword_nothrow));

        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : dwordTestArray)
        {
            verify_set_nothrow<uint32_t>(
                hkey.get(),
                dwordValueName,
                value,
                static_cast<HRESULT(*)(HKEY, PCWSTR, DWORD*)>(wil::reg::get_value_dword_nothrow),
                static_cast<HRESULT(*)(HKEY, PCWSTR, uint32_t)>(wil::reg::set_value_dword_nothrow));
        }

        // fail get* if the value doesn't exist
        DWORD result{};
        auto hr = wil::reg::get_value_dword_nothrow(hkey.get(), (std::wstring(dwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero));
        hr = wil::reg::get_value_dword_nothrow(hkey.get(), qwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_dword_nothrow/get_value_dword_nothrow: with string key")
    {
        for (const auto& value : dwordTestArray)
        {
            verify_set_subkey_nothrow<uint32_t>(
                HKEY_CURRENT_USER,
                testSubkey,
                dwordValueName,
                value,
                static_cast<HRESULT(*)(HKEY, PCWSTR, PCWSTR, DWORD*)>(wil::reg::get_value_dword_nothrow),
                static_cast<HRESULT(*)(HKEY, PCWSTR, PCWSTR, uint32_t)>(wil::reg::set_value_dword_nothrow));
        }

        // fail get* if the value doesn't exist
        DWORD result{};
        auto hr = wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(dwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero));
        hr = wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : dwordTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), dwordValueName, value));
            DWORD result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), dwordValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        DWORD result{};
        auto hr = wil::reg::get_value_nothrow(hkey.get(), (std::wstring(dwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero));
        hr = wil::reg::get_value_nothrow(hkey.get(), qwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with string key")
    {
        for (const auto& value : dwordTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, value));
            DWORD result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        DWORD result{};
        auto hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(dwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero));
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("set_value_dword/get_value_dword: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value_dword(hkey.get(), dwordValueName, value);
            auto result = wil::reg::get_value_dword(hkey.get(), dwordValueName);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_dword(hkey.get(), nullptr, value);
            result = wil::reg::get_value_dword(hkey.get(), nullptr);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_dword(hkey.get(), (std::wstring(dwordValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        wil::reg::set_value_qword(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero);
        try
        {
            wil::reg::get_value_dword(hkey.get(), qwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_dword/get_value_dword: with string key")
    {
        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName, value);
            auto result = wil::reg::get_value_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            result = wil::reg::get_value_dword(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_dword(HKEY_CURRENT_USER, testSubkey, (std::wstring(dwordValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        wil::reg::set_value_qword(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero);
        try
        {
            wil::reg::get_value_dword(HKEY_CURRENT_USER, testSubkey, qwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions")
    {
        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, dwordValueName, value);
            const auto result = wil::reg::get_value<DWORD>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            REQUIRE(result == value);

            wil::reg::set_value_qword(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero);
            // reading the wrong type should fail
            try
            {
                wil::reg::get_value<DWORD>(HKEY_CURRENT_USER, testSubkey, qwordValueName);
                // should throw
                REQUIRE_FALSE(true);
            }
            catch (const wil::ResultException& e)
            {
                REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
            }
        }
    }
    SECTION("get and set with string key and value name, template versions: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value(hkey.get(), dwordValueName, value);
            const auto result = wil::reg::get_value<DWORD>(hkey.get(), dwordValueName);
            REQUIRE(result == value);

            // reading the wrong type should fail
            wil::reg::set_value_qword(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero);
            try
            {
                wil::reg::get_value<DWORD>(hkey.get(), qwordValueName);
                // should throw
                REQUIRE_FALSE(true);
            }
            catch (const wil::ResultException& e)
            {
                REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
            }
        }
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_dword: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value(hkey.get(), dwordValueName, value);
            auto result = wil::reg::try_get_value_dword(hkey.get(), dwordValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value);
            result = wil::reg::try_get_value_dword(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_dword(hkey.get(), (std::wstring(dwordValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        wil::reg::set_value_qword(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero);
        try
        {
            wil::reg::try_get_value_dword(hkey.get(), qwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value/try_get_value_dword: with string key")
    {
        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, dwordValueName, value);
            auto result = wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            result = wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, (std::wstring(dwordValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        wil::reg::set_value_qword(HKEY_CURRENT_USER, testSubkey, qwordValueName, test_qword_zero);
        try
        {
            wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, qwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
#endif
#endif
}

TEST_CASE("BasicRegistryTests::Qwords", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_qword_nothrow/get_value_qword_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : qwordTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(hkey.get(), qwordValueName, value));
            DWORD64 result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(hkey.get(), qwordValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(hkey.get(), nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        DWORD64 result{};
        auto hr = wil::reg::get_value_qword_nothrow(hkey.get(), (std::wstring(qwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_qword_nothrow(hkey.get(), dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_qword_nothrow/get_value_qword_nothrow: with string key")
    {
        for (const auto& value : qwordTestArray)
        {
            verify_set_subkey_nothrow<uint64_t>(
                HKEY_CURRENT_USER,
                testSubkey,
                qwordValueName,
                value,
                static_cast<HRESULT(*)(HKEY, PCWSTR, PCWSTR, DWORD64*)>(wil::reg::get_value_qword_nothrow),
                static_cast<HRESULT(*)(HKEY, PCWSTR, PCWSTR, uint64_t)>(wil::reg::set_value_qword_nothrow));

            REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, value));
            DWORD64 result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        DWORD64 result{};
        auto hr = wil::reg::get_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(qwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : qwordTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), qwordValueName, value));
            DWORD64 result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), qwordValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        DWORD64 result{};
        auto hr = wil::reg::get_value_nothrow(hkey.get(), (std::wstring(qwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(hkey.get(), dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with string key")
    {
        for (const auto& value : qwordTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, value));
            DWORD64 result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        DWORD64 result{};
        auto hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(qwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("set_value_qword/get_value_qword: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : qwordTestArray)
        {
            wil::reg::set_value_qword(hkey.get(), qwordValueName, value);
            auto result = wil::reg::get_value_qword(hkey.get(), qwordValueName);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_qword(hkey.get(), nullptr, value);
            result = wil::reg::get_value_qword(hkey.get(), nullptr);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_qword(hkey.get(), (std::wstring(qwordValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_qword(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_qword/get_value_qword: with string key")
    {
        for (const auto& value : qwordTestArray)
        {
            wil::reg::set_value_qword(HKEY_CURRENT_USER, testSubkey, qwordValueName, value);
            auto result = wil::reg::get_value_qword(HKEY_CURRENT_USER, testSubkey, qwordValueName);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_qword(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            result = wil::reg::get_value_qword(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_qword(HKEY_CURRENT_USER, testSubkey, (std::wstring(qwordValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_qword(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions")
    {
        for (const auto& value : qwordTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, qwordValueName, value);
            const auto result = wil::reg::get_value<DWORD64>(HKEY_CURRENT_USER, testSubkey, qwordValueName);
            REQUIRE(result == value);
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value<DWORD64>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : qwordTestArray)
        {
            wil::reg::set_value(hkey.get(), qwordValueName, value);
            const auto result = wil::reg::get_value<DWORD64>(hkey.get(), qwordValueName);
            REQUIRE(result == value);
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value<DWORD64>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_qword: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : qwordTestArray)
        {
            wil::reg::set_value(hkey.get(), qwordValueName, value);
            auto result = wil::reg::try_get_value_qword(hkey.get(), qwordValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value);
            result = wil::reg::try_get_value_qword(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_qword(hkey.get(), (std::wstring(qwordValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_qword(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value/try_get_value_qword: with string key")
    {
        for (const auto& value : qwordTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, qwordValueName, value);
            auto result = wil::reg::try_get_value_qword(HKEY_CURRENT_USER, testSubkey, qwordValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            result = wil::reg::try_get_value_qword(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_qword(HKEY_CURRENT_USER, testSubkey, (std::wstring(qwordValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_qword(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
#endif
#endif
}

#if defined(WIL_ENABLE_EXCEPTIONS)
TEST_CASE("BasicRegistryTests::wstrings", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_string_nothrow/get_value_string_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, value.c_str()));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), nullptr, result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(hkey.get(), dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_string_nothrow/get_value_string_nothrow: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, value));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(hkey.get(), dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_string/get_value_string: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::get_value_string(hkey.get(), stringValueName);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_string(hkey.get(), nullptr, value.c_str());
            result = wil::reg::get_value_string(hkey.get(), nullptr);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_string(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_string(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_string/get_value_string: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value);
            const auto result = wil::reg::get_value<std::wstring>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == value);
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value<std::wstring>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value);
            const auto result = wil::reg::get_value<std::wstring>(hkey.get(), stringValueName);
            REQUIRE(result == value);
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value <std::wstring>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(_VECTOR_)
    SECTION("get_value_nothrow with non-null-terminated string: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(hkey.get(), stringValueName, REG_SZ, nonNullTerminatedString));

        std::wstring result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
        REQUIRE(result == nonNullTerminatedStringFixed);
    }
    SECTION("get_value_nothrow with non-null-terminated string: with string key")
    {
        REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_SZ, nonNullTerminatedString));

        std::wstring result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result == nonNullTerminatedStringFixed);
    }
    SECTION("get_value_string with non-null-terminated string: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(hkey.get(), stringValueName, REG_SZ, nonNullTerminatedString));

        std::wstring result{ wil::reg::get_value_string(hkey.get(), stringValueName) };
        REQUIRE(result == nonNullTerminatedStringFixed);
    }
    SECTION("get_value_string with non-null-terminated string: with string key")
    {
        REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_SZ, nonNullTerminatedString));

        std::wstring result{ wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName) };
        REQUIRE(result == nonNullTerminatedStringFixed);
    }

    SECTION("get_value_nothrow with empty string value: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(hkey.get(), stringValueName, REG_SZ, emptyStringTestValue));

        std::wstring result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
        REQUIRE(result.empty());
    }
    SECTION("get_value_nothrow with empty string value: with string key")
    {
        REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_SZ, emptyStringTestValue));

        std::wstring result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result.empty());
    }
    SECTION("get_value_string with empty string value: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(hkey.get(), stringValueName, REG_SZ, emptyStringTestValue));

        std::wstring result{ wil::reg::get_value_string(hkey.get(), stringValueName) };
        REQUIRE(result.empty());
    }
    SECTION("get_value_string with empty string value: with string key")
    {
        REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_SZ, emptyStringTestValue));

        std::wstring result{ wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName) };
        REQUIRE(result.empty());
    }

#endif

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_string: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_string(hkey.get(), stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // verify ways to copy or move values out of the returned value
            const auto copy_result = result.value_or(L"Test");
            const auto move_result = wil::reg::try_get_value_string(hkey.get(), stringValueName).value_or(L"test");

            // potentially moves out of result - cannot use result afterwards
            const auto move_result2 = static_cast<std::optional<std::wstring>&&>(result).value_or(L"Test");

            const auto empty_result = wil::reg::try_get_value_string(hkey.get(), stringValueName).value_or(std::wstring{});

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value.c_str());
            result = wil::reg::try_get_value_string(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_string(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_string(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value/try_get_value_string: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
#endif

    SECTION("set_value_nothrow/get_value_string_nothrow: with open key")
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
        DWORD expectedSize{};
        auto hr = wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA));
        REQUIRE(expectedSize == 12);
        WCHAR valid_buffer_result[5]{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, valid_buffer_result, &expectedSize));
        REQUIRE(expectedSize == 10);
        REQUIRE(0 == wcscmp(valid_buffer_result, L"Test"));

        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(expectedSize == 0);

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(hkey.get(), dwordValueName, too_small_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_string_nothrow: with string key")
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
        DWORD expectedSize{};
        auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA));
        REQUIRE(expectedSize == 12); // yes, this is a registry oddity that it returned 2-bytes-more-than-required
        WCHAR valid_buffer_result[5]{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, valid_buffer_result, &expectedSize));
        REQUIRE(expectedSize == 10);
        REQUIRE(0 == wcscmp(valid_buffer_result, L"Test"));

        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(expectedSize == 0);

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, too_small_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
}
#endif

#if defined(__WIL_OLEAUTO_H_)
TEST_CASE("BasicRegistryTests::bstrs", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_string_nothrow/get_value_string_nothrow<::wil::unique_bstr>: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, value.c_str()));
            wil::unique_bstr result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::unique_bstr result{};
        auto hr = wil::reg::get_value_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(hkey.get(), dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_string_nothrow/get_value_string_nothrow<::wil::unique_bstr>: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::unique_bstr result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::unique_bstr result{};
        auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, value.c_str()));
            wil::unique_bstr result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::unique_bstr result{};
        auto hr = wil::reg::get_value_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(hkey.get(), dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::unique_bstr result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::unique_bstr result{};
        auto hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("set_value_string/get_value_string<::wil::unique_bstr>: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::get_value_string<::wil::unique_bstr>(hkey.get(), stringValueName);
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            wil::reg::set_value_string(hkey.get(), nullptr, value.c_str());
            result = wil::reg::get_value_string<::wil::unique_bstr>(hkey.get(), nullptr);
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_string<::wil::unique_bstr>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_string<::wil::unique_bstr>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_string/get_value_string<::wil::unique_bstr>: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::get_value_string<::wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::get_value_string<::wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_string<::wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_string<::wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value);
            const auto result = wil::reg::get_value<wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(AreStringsEqual(result, value));
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value<wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value);
            const auto result = wil::reg::get_value<wil::unique_bstr>(hkey.get(), stringValueName);
            REQUIRE(AreStringsEqual(result, value));
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value <wil::unique_bstr>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(__WIL_OLEAUTO_H_STL)
    SECTION("get and set with string key and value name, template versions")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value);
            const auto result = wil::reg::get_value<wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(AreStringsEqual(result, value));
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value<wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value);
            const auto result = wil::reg::get_value<wil::shared_bstr>(hkey.get(), stringValueName);
            REQUIRE(AreStringsEqual(result, value));
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value <wil::shared_bstr>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_string_nothrow/get_value_string_nothrow<::wil::shared_bstr>: shared_bstr with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, value.c_str()));
            wil::shared_bstr shared_result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, shared_result));
            REQUIRE(AreStringsEqual(shared_result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), nullptr, value.c_str()));
            shared_result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), nullptr, shared_result));
            REQUIRE(AreStringsEqual(shared_result, value));
        }

        // fail get* if the value doesn't exist
        wil::shared_bstr shared_result{};
        auto hr = wil::reg::get_value_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), shared_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(hkey.get(), dwordValueName, shared_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_string_nothrow/get_value_string_nothrow<::wil::shared_bstr>: shared_bstr with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::shared_bstr shared_result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, shared_result));
            REQUIRE(AreStringsEqual(shared_result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            shared_result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, shared_result));
            REQUIRE(AreStringsEqual(shared_result, value));
        }

        // fail get* if the value doesn't exist
        wil::shared_bstr shared_result{};
        auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), shared_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, shared_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_string<::wil::shared_bstr>: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_string<::wil::shared_bstr>(hkey.get(), stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), value));

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value.c_str());
            result = wil::reg::try_get_value_string<::wil::shared_bstr>(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), value));
        }

        // fail get* if the value doesn't exist
        auto result = wil::reg::try_get_value_string<::wil::shared_bstr>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        const auto empty_result2 = wil::reg::try_get_value_string<::wil::shared_bstr>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str()).value_or(wil::unique_bstr{});
        REQUIRE(!empty_result2);

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_string<::wil::shared_bstr>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value/try_get_value_string<::wil::shared_bstr>: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_string<::wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), value));

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::try_get_value_string<::wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), value));
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_string<::wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_string<::wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
#endif // #if defined(__cpp_lib_optional)
#endif // #if defined(__WIL_OLEAUTO_H_STL)
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)
}
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
TEST_CASE("BasicRegistryTests::cotaskmem_string", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_string_nothrow/get_value_string_nothrow (wil::unique_cotaskmem_string): with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, value.c_str()));
            wil::unique_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), nullptr, stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), nullptr, nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::unique_cotaskmem_string result{};
        auto hr = wil::reg::get_value_string_nothrow(hkey.get(), nullptr, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(hkey.get(), nullptr, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_string_nothrow/get_value_string_nothrow (wil::unique_cotaskmem_string): with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::unique_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::unique_cotaskmem_string result{};
        auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow (wil::unique_cotaskmem_string): with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, value.c_str()));
            wil::unique_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::unique_cotaskmem_string result{};
        auto hr = wil::reg::get_value_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(hkey.get(), dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow (wil::unique_cotaskmem_string): with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::unique_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::unique_cotaskmem_string result{};
        auto hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#if defined(__WIL_OBJBASE_H_STL)
    SECTION("set_value_string_nothrow/get_value_string_nothrow (wil::shared_cotaskmem_string): with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, value.c_str()));
            wil::shared_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), nullptr, stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), nullptr, nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::shared_cotaskmem_string result{};
        auto hr = wil::reg::get_value_string_nothrow(hkey.get(), nullptr, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(hkey.get(), nullptr, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_string_nothrow/get_value_string_nothrow (wil::shared_cotaskmem_string): with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::shared_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::shared_cotaskmem_string result{};
        auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow (wil::shared_cotaskmem_string): with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, value.c_str()));
            wil::shared_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::shared_cotaskmem_string result{};
        auto hr = wil::reg::get_value_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(hkey.get(), dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow (wil::shared_cotaskmem_string): with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::shared_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        wil::shared_cotaskmem_string result{};
        auto hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
#endif // #if defined(__WIL_OBJBASE_H_STL)

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("set_value_string/get_value_string (wil::unique_cotaskmem_string): with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::get_value_string<wil::unique_cotaskmem_string>(hkey.get(), stringValueName);
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            wil::reg::set_value_string(hkey.get(), nullptr, value.c_str());
            result = wil::reg::get_value_string<wil::unique_cotaskmem_string>(hkey.get(), nullptr);
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_string<wil::unique_cotaskmem_string>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_string<wil::unique_cotaskmem_string>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_string/get_value_string (wil::unique_cotaskmem_string): with string key")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::get_value_string<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::get_value_string<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_string<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_string<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value);
            const auto result = wil::reg::get_value<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(AreStringsEqual(result, value));
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value);
            const auto result = wil::reg::get_value<wil::unique_cotaskmem_string>(hkey.get(), stringValueName);
            REQUIRE(AreStringsEqual(result, value));
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value <wil::unique_cotaskmem_string>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(__WIL_OBJBASE_H_STL)
    SECTION("set_value_string/get_value_string (wil::shared_cotaskmem_string): with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::get_value_string<wil::shared_cotaskmem_string>(hkey.get(), stringValueName);
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            wil::reg::set_value_string(hkey.get(), nullptr, value.c_str());
            result = wil::reg::get_value_string<wil::shared_cotaskmem_string>(hkey.get(), nullptr);
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_string<wil::shared_cotaskmem_string>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_string<wil::shared_cotaskmem_string>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_string/get_value_string (wil::shared_cotaskmem_string): with string key")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::get_value_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::get_value_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value);
            const auto result = wil::reg::get_value<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(AreStringsEqual(result, value));
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("get and set with string key and value name, template versions: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value);
            const auto result = wil::reg::get_value<wil::shared_cotaskmem_string>(hkey.get(), stringValueName);
            REQUIRE(AreStringsEqual(result, value));
        }

        // reading the wrong type should fail
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value <wil::shared_cotaskmem_string>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_string (wil::shared_cotaskmem_string): with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(hkey.get(), stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), value));

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value.c_str());
            result = wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), value));
        }

        // fail get* if the value doesn't exist
        auto result = wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        auto empty_result2 = wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str()).value_or(wil::unique_cotaskmem_string{});
        REQUIRE(!empty_result2);

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value/try_get_value_string (wil::shared_cotaskmem_string): with string key")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), value));

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), value));
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
#endif // #if defined(__cpp_lib_optional)
#endif // #if defined(__WIL_OBJBASE_H_STL)
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)
}
#endif // #if defined(__WIL_OBJBASE_H_)

#if defined(WIL_ENABLE_EXCEPTIONS)
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
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(result == expanded_value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), nullptr, result));
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

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
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(result == expanded_value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
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
        REQUIRE(expectedSize == 22);
        WCHAR valid_buffer_result[11]{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, valid_buffer_result, &expectedSize));
        REQUIRE(expectedSize == 22);

        expectedSize = 0;
        WCHAR expanded_value[test_expanded_string_buffer_size]{};
        const auto expanded_result = ::ExpandEnvironmentStringsW(L"%WINDIR%", expanded_value, test_expanded_string_buffer_size);
        REQUIRE(expanded_result != ERROR_SUCCESS);
        REQUIRE(expanded_result < test_expanded_string_buffer_size);
        REQUIRE(0 == wcscmp(valid_buffer_result, expanded_value));


        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

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
        REQUIRE(expectedSize == 22);

        expectedSize = 0;
        WCHAR valid_buffer_result[11]{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, valid_buffer_result, &expectedSize));
        REQUIRE(expectedSize == 22);

        WCHAR expanded_value[test_expanded_string_buffer_size]{};
        const auto expanded_result = ::ExpandEnvironmentStringsW(L"%WINDIR%", expanded_value, test_expanded_string_buffer_size);
        REQUIRE(expanded_result != ERROR_SUCCESS);
        REQUIRE(expanded_result < test_expanded_string_buffer_size);
        REQUIRE(0 == wcscmp(valid_buffer_result, expanded_value));

        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_expanded_string/get_value_expanded_string: with open key")
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

            wil::reg::set_value_expanded_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::get_value_expanded_string(hkey.get(), stringValueName);
            REQUIRE(result == expanded_value);

            // and verify default value name
            wil::reg::set_value_expanded_string(hkey.get(), nullptr, value.c_str());
            result = wil::reg::get_value_expanded_string(hkey.get(), nullptr);
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_expanded_string(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_expanded_string(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_expanded_string/get_value_expanded_string: with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == expanded_value);

            // and verify default value name
            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_expanded_string: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            wil::reg::set_value_expanded_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_expanded_string(hkey.get(), stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == expanded_value);

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value.c_str());
            result = wil::reg::try_get_value_expanded_string(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_expanded_string(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_expanded_string(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value/try_get_value_expanded_string: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == expanded_value);

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::try_get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_expanded_string(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
#endif
}
#endif

#if defined(WIL_ENABLE_EXCEPTIONS) && defined(__WIL_OLEAUTO_H_)
TEST_CASE("BasicRegistryTests::expanded_bstr", "[registry]")
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
            wil::unique_bstr result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), nullptr, result));
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        wil::unique_bstr result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

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
            wil::unique_bstr result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        wil::unique_bstr result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#if defined(__WIL_OLEAUTO_H_STL)
    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow (wil::shared_bstr): with opened key")
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
            wil::shared_bstr shared_result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), stringValueName, shared_result));
            REQUIRE(AreStringsEqual(shared_result, expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), nullptr, value.c_str()));
            shared_result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), nullptr, shared_result));
            REQUIRE(AreStringsEqual(shared_result, expanded_value));
        }

        // fail get* if the value doesn't exist
        wil::shared_bstr shared_result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), shared_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), dwordValueName, shared_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow (wil::shared_bstr): with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::shared_bstr shared_result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, shared_result));
            REQUIRE(AreStringsEqual(shared_result, expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            shared_result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, shared_result));
            REQUIRE(AreStringsEqual(shared_result, expanded_value));
        }

        // fail get* if the value doesn't exist
        wil::shared_bstr shared_result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), shared_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, shared_result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
#endif // #if defined(__WIL_OLEAUTO_H_STL)

    SECTION("set_value_expanded_string/get_value_expanded_string<wil::unique_bstr>: with open key")
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

            wil::reg::set_value_expanded_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::get_value_expanded_string<wil::unique_bstr>(hkey.get(), stringValueName);
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            wil::reg::set_value_expanded_string(hkey.get(), nullptr, value.c_str());
            result = wil::reg::get_value_expanded_string<wil::unique_bstr>(hkey.get(), nullptr);
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_expanded_string<wil::unique_bstr>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_expanded_string<wil::unique_bstr>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_expanded_string/get_value_expanded_string<wil::unique_bstr>: with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::get_value_expanded_string<wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::get_value_expanded_string<wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_expanded_string<wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_expanded_string<wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_string<wil::shared_bstr>: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            wil::reg::set_value_expanded_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_string<wil::shared_bstr>(hkey.get(), stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), expanded_value));

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value.c_str());
            result = wil::reg::try_get_value_string<wil::shared_bstr>(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), expanded_value));
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_string<wil::shared_bstr>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_string<wil::shared_bstr>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value/try_get_value_string<wil::shared_bstr>: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_string<wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), expanded_value));

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::try_get_value_string<wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), expanded_value));
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_string<wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_string<wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
#endif
}
#endif // #if defined(WIL_ENABLE_EXCEPTIONS) && defined(__WIL_OLEAUTO_H_)

#if defined(WIL_ENABLE_EXCEPTIONS) && defined(__WIL_OLEAUTO_H_)
TEST_CASE("BasicRegistryTests::expanded_cotaskmem_string", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow (wil::unique_cotaskmem_string): with opened key")
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
            wil::unique_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), nullptr, result));
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        wil::unique_cotaskmem_string result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow (wil::unique_cotaskmem_string): with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::unique_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        wil::unique_cotaskmem_string result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#if defined(__WIL_OBJBASE_H_STL)
    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow (wil::shared_cotaskmem_string): with opened key")
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
            wil::shared_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), stringValueName, result));
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(hkey.get(), nullptr, result));
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        wil::shared_cotaskmem_string result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow (wil::shared_cotaskmem_string): with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            wil::shared_cotaskmem_string result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, result));
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, result));
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        wil::shared_cotaskmem_string result{};
        auto hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
#endif

    SECTION("set_value_expanded_string/get_value_expanded_string (wil::unique_cotaskmem_string): with open key")
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

            wil::reg::set_value_expanded_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::get_value_expanded_string<wil::unique_cotaskmem_string>(hkey.get(), stringValueName);
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            wil::reg::set_value_expanded_string(hkey.get(), nullptr, value.c_str());
            result = wil::reg::get_value_expanded_string<wil::unique_cotaskmem_string>(hkey.get(), nullptr);
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_expanded_string<wil::unique_cotaskmem_string>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_expanded_string<wil::unique_cotaskmem_string>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_expanded_string/get_value_expanded_string (wil::unique_cotaskmem_string(>): with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::get_value_expanded_string<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::get_value_expanded_string<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_expanded_string<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_expanded_string<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(__WIL_OBJBASE_H_STL) && defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_expanded_string (wil::shared_cotaskmem_string): with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            wil::reg::set_value_expanded_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_expanded_string<wil::shared_cotaskmem_string>(hkey.get(), stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), expanded_value));

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value.c_str());
            result = wil::reg::try_get_value_expanded_string<wil::shared_cotaskmem_string>(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), expanded_value));
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_expanded_string<wil::shared_cotaskmem_string>(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_expanded_string<wil::shared_cotaskmem_string>(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value/try_get_value_expanded_string (wil::shared_cotaskmem_string): with string key")
    {
        for (const auto& value : stringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[test_expanded_string_buffer_size]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, test_expanded_string_buffer_size);
            REQUIRE(expanded_result != ERROR_SUCCESS);
            REQUIRE(expanded_result < test_expanded_string_buffer_size);

            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_expanded_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), expanded_value));

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::try_get_value_expanded_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(AreStringsEqual(result.value(), expanded_value));
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_expanded_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_expanded_string<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
#endif // #if defined(__WIL_OBJBASE_H_STL) && defined(__cpp_lib_optional)
}
#endif // #if defined(WIL_ENABLE_EXCEPTIONS) && defined(__WIL_OLEAUTO_H_)

#ifdef __WIL_WINREG_STL
TEST_CASE("BasicRegistryTests::multi-strings", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_multistring_nothrow/get_value_multistring_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : multiStringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(hkey.get(), stringValueName, value));
            std::vector<std::wstring> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(hkey.get(), stringValueName, &result));
            // if value == empty, we wrote in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
            // thus the result should have one empty string
            auto adjustedValue = value;
            if (adjustedValue.empty())
            {
                adjustedValue.resize(1);
            }
            REQUIRE(result == adjustedValue);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(hkey.get(), nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == adjustedValue);
        }

        // fail get* if the value doesn't exist
        std::vector<std::wstring> result{};
        auto hr = wil::reg::get_value_multistring_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_multistring_nothrow(hkey.get(), dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_multistring_nothrow/get_value_multistring_nothrow: with string key")
    {
        for (const auto& value : multiStringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
            std::vector<std::wstring> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
            // if value == empty, we wrote in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
            // thus the result should have one empty string
            auto adjustedValue = value;
            if (adjustedValue.empty())
            {
                adjustedValue.resize(1);
            }
            REQUIRE(result == adjustedValue);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == adjustedValue);
        }

        // fail get* if the value doesn't exist
        std::vector<std::wstring> result{};
        auto hr = wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : multiStringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, value));
            std::vector<std::wstring> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
            // if value == empty, we wrote in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
            // thus the result should have one empty string
            auto adjustedValue = value;
            if (adjustedValue.empty())
            {
                adjustedValue.resize(1);
            }
            REQUIRE(result == adjustedValue);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == adjustedValue);
        }

        // fail get* if the value doesn't exist
        std::vector<std::wstring> result{};
        auto hr = wil::reg::get_value_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(hkey.get(), dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with string key")
    {
        for (const auto& value : multiStringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
            std::vector<std::wstring> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
            // if value == empty, we wrote in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
            // thus the result should have one empty string
            auto adjustedValue = value;
            if (adjustedValue.empty())
            {
                adjustedValue.resize(1);
            }
            REQUIRE(result == adjustedValue);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == adjustedValue);
        }

        // fail get* if the value doesn't exist
        std::vector<std::wstring> result{};
        auto hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("set_value_multistring/get_value_multistring: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : multiStringTestArray)
        {
            wil::reg::set_value_multistring(hkey.get(), stringValueName, value);
            auto result = wil::reg::get_value_multistring(hkey.get(), stringValueName);
            // if value == empty, we wrote in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
            // thus the result should have one empty string
            auto adjustedValue = value;
            if (adjustedValue.empty())
            {
                adjustedValue.resize(1);
            }
            REQUIRE(result == adjustedValue);

            // and verify default value name
            wil::reg::set_value_multistring(hkey.get(), nullptr, value);
            result = wil::reg::get_value_multistring(hkey.get(), nullptr);
            REQUIRE(result == adjustedValue);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_multistring(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_multistring(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_multistring/get_value_multistring: with string key")
    {
        for (const auto& value : multiStringTestArray)
        {
            wil::reg::set_value_multistring(HKEY_CURRENT_USER, testSubkey, stringValueName, value);
            auto result = wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, stringValueName);
            // if value == empty, we wrote in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
            // thus the result should have one empty string
            auto adjustedValue = value;
            if (adjustedValue.empty())
            {
                adjustedValue.resize(1);
            }
            REQUIRE(result == adjustedValue);

            // and verify default value name
            wil::reg::set_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            result = wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == adjustedValue);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_multistring: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : multiStringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value);
            auto result = wil::reg::try_get_value_multistring(hkey.get(), stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            // if value == empty, we wrote in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
            // thus the result should have one empty string
            auto adjustedValue = value;
            if (adjustedValue.empty())
            {
                adjustedValue.resize(1);
            }
            REQUIRE(result == adjustedValue);

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value);
            result = wil::reg::try_get_value_multistring(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == adjustedValue);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_multistring(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_multistring(hkey.get(), dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

    SECTION("set_value/try_get_value_multistring: with string key")
    {
        for (const auto& value : multiStringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value);
            auto result = wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            // if value == empty, we wrote in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
            // thus the result should have one empty string
            auto adjustedValue = value;
            if (adjustedValue.empty())
            {
                adjustedValue.resize(1);
            }
            REQUIRE(result == adjustedValue);

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            result = wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == adjustedValue);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        try
        {
            wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
#endif
#endif
}
#endif

#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
TEST_CASE("BasicRegistryTests::vector-bytes", "[registry]]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_byte_vector_nothrow/get_value_byte_vector_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : vectorBytesTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(hkey.get(), stringValueName, REG_BINARY, value));
            std::vector<BYTE> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_byte_vector_nothrow(hkey.get(), stringValueName, REG_BINARY, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(hkey.get(), nullptr, REG_BINARY, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_byte_vector_nothrow(hkey.get(), nullptr, REG_BINARY, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::vector<BYTE> result{};
        auto hr = wil::reg::get_value_byte_vector_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), REG_BINARY, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        hr = wil::reg::get_value_byte_vector_nothrow(hkey.get(), stringValueName, REG_SZ, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        hr = wil::reg::get_value_byte_vector_nothrow(hkey.get(), stringValueName, REG_DWORD, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));

        // should succeed if we specify the correct type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, 0xffffffff));
        REQUIRE_SUCCEEDED(wil::reg::get_value_byte_vector_nothrow(hkey.get(), dwordValueName, REG_DWORD, &result));
        REQUIRE(result.size() == 4);
        REQUIRE(result[0] == 0xff);
        REQUIRE(result[1] == 0xff);
        REQUIRE(result[2] == 0xff);
        REQUIRE(result[3] == 0xff);
    }
    SECTION("set_value_byte_vector_nothrow/get_value_byte_vector_nothrow: with string key")
    {
        for (const auto& value : vectorBytesTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_BINARY, value));
            std::vector<BYTE> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_BINARY, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, REG_BINARY, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, REG_BINARY, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::vector<BYTE> result{};
        auto hr = wil::reg::get_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), REG_BINARY, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        hr = wil::reg::get_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_SZ, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        hr = wil::reg::get_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_DWORD, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));

        // should succeed if we specify the correct type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, 0xffffffff));
        REQUIRE_SUCCEEDED(wil::reg::get_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, REG_DWORD, &result));
        REQUIRE(result.size() == 4);
        REQUIRE(result[0] == 0xff);
        REQUIRE(result[1] == 0xff);
        REQUIRE(result[2] == 0xff);
        REQUIRE(result[3] == 0xff);
    }
    SECTION("set_value_byte_vector/get_value_byte_vector: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : vectorBytesTestArray)
        {
            wil::reg::set_value_byte_vector(hkey.get(), stringValueName, REG_BINARY, value);
            auto result = wil::reg::get_value_byte_vector(hkey.get(), stringValueName, REG_BINARY);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_byte_vector(hkey.get(), nullptr, REG_BINARY, value);
            result = wil::reg::get_value_byte_vector(hkey.get(), nullptr, REG_BINARY);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_byte_vector(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), REG_BINARY);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, 0xffffffff));
        try
        {
            wil::reg::get_value_byte_vector(hkey.get(), dwordValueName, REG_BINARY);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }

        // should succeed if we specify the correct type
        auto result = wil::reg::get_value_byte_vector(hkey.get(), dwordValueName, REG_DWORD);
        REQUIRE(result.size() == 4);
        REQUIRE(result[0] == 0xff);
        REQUIRE(result[1] == 0xff);
        REQUIRE(result[2] == 0xff);
        REQUIRE(result[3] == 0xff);
    }
    SECTION("set_value_byte_vector/get_value_byte_vector: with string key")
    {
        for (const auto& value : vectorBytesTestArray)
        {
            wil::reg::set_value_byte_vector(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_BINARY, value);
            auto result = wil::reg::get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_BINARY);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_byte_vector(HKEY_CURRENT_USER, testSubkey, nullptr, REG_BINARY, value);
            result = wil::reg::get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, nullptr, REG_BINARY);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), REG_BINARY);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, 0xffffffff));
        try
        {
            wil::reg::get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, dwordValueName, REG_BINARY);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }

        // should succeed if we specify the correct type
        auto result = wil::reg::get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, dwordValueName, REG_DWORD);
        REQUIRE(result.size() == 4);
        REQUIRE(result[0] == 0xff);
        REQUIRE(result[1] == 0xff);
        REQUIRE(result[2] == 0xff);
        REQUIRE(result[3] == 0xff);
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value_byte_vector/try_get_value_byte_vector: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        for (const auto& value : vectorBytesTestArray)
        {
            wil::reg::set_value_byte_vector(hkey.get(), stringValueName, REG_BINARY, value);
            auto result = wil::reg::try_get_value_byte_vector(hkey.get(), stringValueName, REG_BINARY);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_byte_vector(hkey.get(), nullptr, REG_BINARY, value);
            result = wil::reg::try_get_value_byte_vector(hkey.get(), nullptr, REG_BINARY);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        auto result = wil::reg::try_get_value_byte_vector(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), REG_BINARY);
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, 0xffffffff));
        try
        {
            wil::reg::try_get_value_byte_vector(hkey.get(), dwordValueName, REG_BINARY);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }

        // should succeed if we specify the correct type
        result = wil::reg::try_get_value_byte_vector(hkey.get(), dwordValueName, REG_DWORD);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 4);
        REQUIRE(result->at(0) == 0xff);
        REQUIRE(result->at(1) == 0xff);
        REQUIRE(result->at(2) == 0xff);
        REQUIRE(result->at(3) == 0xff);
    }

    SECTION("set_value/try_get_value_byte_vector: with string key")
    {
        for (const auto& value : vectorBytesTestArray)
        {
            wil::reg::set_value_byte_vector(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_BINARY, value);
            auto result = wil::reg::try_get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, stringValueName, REG_BINARY);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_byte_vector(HKEY_CURRENT_USER, testSubkey, nullptr, REG_BINARY, value);
            result = wil::reg::try_get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, nullptr, REG_BINARY);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        auto result = wil::reg::try_get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), REG_BINARY);
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, 0xffffffff));
        try
        {
            wil::reg::try_get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, dwordValueName, REG_BINARY);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }

        // should succeed if we specify the correct type
        result = wil::reg::try_get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, dwordValueName, REG_DWORD);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 4);
        REQUIRE(result->at(0) == 0xff);
        REQUIRE(result->at(1) == 0xff);
        REQUIRE(result->at(2) == 0xff);
        REQUIRE(result->at(3) == 0xff);
    }
#endif
}
#endif