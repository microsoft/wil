#include <memory>
#include <string>
#include <vector>
#include <optional>

#include <windows.h>
#include <sddl.h>

#include <wil/filesystem.h>
#include <wil/registry_basic.h>
#include <wil/resource.h>

#include "common.h"

constexpr auto* testSubkey = L"Software\\Microsoft\\BasicRegistryTest";
constexpr auto* dwordValueName = L"MyDwordValue";
constexpr auto* qwordValueName = L"MyQwordvalue";
constexpr auto* stringValueName = L"MyStringValue";

constexpr DWORD dwordTestArray[] = { static_cast<DWORD>(-1), 1, 0 };
constexpr DWORD64 qwordTestArray[] = { static_cast<DWORD64>(-1), 1, 0 };
const std::wstring stringTestArray[] = { L".", L"", L"Hello there!", L"\0" };
const std::wstring expandedStringTestArray[] = { L".", L"", L"%WINDIR%", L"\0" };
const std::vector<std::wstring> multiStringTestArray[]{
    { {L"Hello there!"}, {L"Hello a second time!"}, {L"Hello a third time!"} },
    { {L""}, {L""}, {L""} },
    {},
    { {L"a"} }
};

TEST_CASE("BasicRegistryTests::Open", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("open_key_nothrow: with opened key")
    {
        constexpr auto* subSubKey = L"subkey";

        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));
        // create a sub-key under this which we will try to open - but open_key will use the above hkey
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(hkey.get(), subSubKey, &subkey, wil::reg::key_access::readwrite));
        // write a test value we'll try to read from later
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(subkey.get(), dwordValueName, 2));

        wil::unique_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_key_nothrow(hkey.get(), subSubKey, &opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_key_nothrow(hkey.get(), subSubKey, &opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == 2);
        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, 3);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_key_nothrow(hkey.get(), subSubKey, &opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, 3));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == 3);

        // fail open if the key doesn't exist
        hr = wil::reg::open_key_nothrow(hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), &opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    }

    SECTION("open_key_nothrow: with string key")
    {
        // create read-write, should be able to open read and open read-write
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));
        // write a test value
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, 2));

        wil::unique_hkey opened_key;

        REQUIRE_SUCCEEDED(wil::reg::open_key_nothrow(HKEY_CURRENT_USER, testSubkey, &opened_key, wil::reg::key_access::read));

        REQUIRE_SUCCEEDED(wil::reg::open_key_nothrow(HKEY_CURRENT_USER, testSubkey, &opened_key, wil::reg::key_access::read));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == 2);
        auto hr = wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, 3);
        REQUIRE(hr == E_ACCESSDENIED);

        REQUIRE_SUCCEEDED(wil::reg::open_key_nothrow(HKEY_CURRENT_USER, testSubkey, &opened_key, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(opened_key.get(), dwordValueName, 3));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(opened_key.get(), dwordValueName, &result));
        REQUIRE(result == 3);

        // fail open if the key doesn't exist
        hr = wil::reg::open_key_nothrow(HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), &opened_key, wil::reg::key_access::read);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("open_unique_key: with opened key")
    {
        constexpr auto* subSubKey = L"subkey";

        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));
        // create a sub-key under this which we will try to open - but open_key will use the above hkey
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(hkey.get(), subSubKey, &subkey, wil::reg::key_access::readwrite));
        // write a test value we'll try to read from later
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(subkey.get(), dwordValueName, 2));

        wil::unique_hkey read_only_key{ wil::reg::open_unique_key(hkey.get(), subSubKey, wil::reg::key_access::read) };
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(read_only_key.get(), dwordValueName, &result));
        REQUIRE(result == 2);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, 3);
        REQUIRE(hr == E_ACCESSDENIED);

        wil::unique_hkey read_write_key{ wil::reg::open_unique_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite) };
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(read_write_key.get(), dwordValueName, 3));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(read_write_key.get(), dwordValueName, &result));
        REQUIRE(result == 3);

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
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));
        // write a test value we'll try to read from later
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, 2));

        wil::unique_hkey read_only_key{ wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::read) };
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(read_only_key.get(), dwordValueName, &result));
        REQUIRE(result == 2);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, 3);
        REQUIRE(hr == E_ACCESSDENIED);

        wil::unique_hkey read_write_key{ wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite) };
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(read_write_key.get(), dwordValueName, 3));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(read_write_key.get(), dwordValueName, &result));
        REQUIRE(result == 3);

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

        wil::shared_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));
        // create a sub-key under this which we will try to open - but open_key will use the above hkey
        wil::shared_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(hkey.get(), subSubKey, &subkey, wil::reg::key_access::readwrite));
        // write a test value we'll try to read from later
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(subkey.get(), dwordValueName, 2));

        wil::shared_hkey read_only_key{ wil::reg::open_shared_key(hkey.get(), subSubKey, wil::reg::key_access::read) };
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(read_only_key.get(), dwordValueName, &result));
        REQUIRE(result == 2);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, 3);
        REQUIRE(hr == E_ACCESSDENIED);

        wil::shared_hkey read_write_key{ wil::reg::open_shared_key(hkey.get(), subSubKey, wil::reg::key_access::readwrite) };
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(read_write_key.get(), dwordValueName, 3));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(read_write_key.get(), dwordValueName, &result));
        REQUIRE(result == 3);

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
        wil::shared_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));
        // write a test value we'll try to read from later
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, 2));

        wil::shared_hkey read_only_key{ wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::read) };
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(read_only_key.get(), dwordValueName, &result));
        REQUIRE(result == 2);
        auto hr = wil::reg::set_value_dword_nothrow(read_only_key.get(), dwordValueName, 3);
        REQUIRE(hr == E_ACCESSDENIED);

        wil::shared_hkey read_write_key{ wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite) };
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(read_write_key.get(), dwordValueName, 3));
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(read_write_key.get(), dwordValueName, &result));
        REQUIRE(result == 3);

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

TEST_CASE("BasicRegistryTests::Dwords", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_dword_nothrow/get_value_dword_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : dwordTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, value));
            DWORD result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(hkey.get(), dwordValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        DWORD result{};
        auto hr = wil::reg::get_value_dword_nothrow(hkey.get(), (std::wstring(dwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        wchar_t stringResult[100]{};
        hr = wil::reg::get_value_string_nothrow(hkey.get(), dwordValueName, stringResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_dword_nothrow/get_value_dword_nothrow: with string key")
    {
        for (const auto& value : dwordTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, value));
            DWORD result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        DWORD result{};
        auto hr = wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(dwordValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        wchar_t stringResult[100]{};
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, stringResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    SECTION("set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

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
        wchar_t stringResult[100]{};
        hr = wil::reg::get_value_nothrow(hkey.get(), dwordValueName, stringResult);
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
        wchar_t stringResult[100]{};
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, stringResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("set_value_dword/get_value_dword: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

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
        try
        {
            wil::reg::get_value_wstring(hkey.get(), dwordValueName);
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
        try
        {
            wil::reg::get_value_wstring(HKEY_CURRENT_USER, testSubkey, dwordValueName);
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

            // reading the wrong type should fail
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
    }

    SECTION("get and set with string key and value name, template versions: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value(hkey.get(), dwordValueName, value);
            const auto result = wil::reg::get_value<DWORD>(hkey.get(), dwordValueName);
            REQUIRE(result == value);

            // reading the wrong type should fail
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
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_dword: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

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
        try
        {
            wil::reg::try_get_value_wstring(hkey.get(), dwordValueName);
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
        try
        {
            wil::reg::try_get_value_wstring(HKEY_CURRENT_USER, testSubkey, dwordValueName);
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
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

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
        wchar_t stringResult[100]{};
        hr = wil::reg::get_value_string_nothrow(hkey.get(), qwordValueName, stringResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_qword_nothrow/get_value_qword_nothrow: with string key")
    {
        for (const auto& value : qwordTestArray)
        {
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
        wchar_t stringResult[100]{};
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, stringResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    SECTION("set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

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
        wchar_t stringResult[100]{};
        hr = wil::reg::get_value_nothrow(hkey.get(), qwordValueName, stringResult);
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
        wchar_t stringResult[100]{};
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, qwordValueName, stringResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("set_value_qword/get_value_qword: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

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
        try
        {
            wil::reg::get_value_wstring(hkey.get(), qwordValueName);
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
        try
        {
            wil::reg::get_value_wstring(HKEY_CURRENT_USER, testSubkey, qwordValueName);
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

    SECTION("get and set with string key and value name, template versions: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : qwordTestArray)
        {
            wil::reg::set_value(hkey.get(), qwordValueName, value);
            const auto result = wil::reg::get_value<DWORD64>(hkey.get(), qwordValueName);
            REQUIRE(result == value);
        }

        // reading the wrong type should fail
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

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_qword: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

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
        try
        {
            wil::reg::try_get_value_wstring(hkey.get(), qwordValueName);
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
        try
        {
            wil::reg::try_get_value_wstring(HKEY_CURRENT_USER, testSubkey, qwordValueName);
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

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("BasicRegistryTests::wstrings", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_string_nothrow/get_value_wstring_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), stringValueName, value.c_str()));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_wstring_nothrow(hkey.get(), stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_wstring_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_wstring_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_dword_nothrow(hkey.get(), stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_string_nothrow/get_value_wstring_nothrow: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    SECTION("set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, value.c_str()));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_nothrow(hkey.get(), stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_nothrow: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    SECTION("set_value_string/get_value_string: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::get_value_wstring(hkey.get(), stringValueName);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_string(hkey.get(), nullptr, value.c_str());
            result = wil::reg::get_value_wstring(hkey.get(), nullptr);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_wstring(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        try
        {
            wil::reg::get_value_dword(hkey.get(), stringValueName);
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
            auto result = wil::reg::get_value_wstring(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::get_value_wstring(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_wstring(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        try
        {
            wil::reg::get_value_dword(HKEY_CURRENT_USER, testSubkey, stringValueName);
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
        try
        {
            wil::reg::get_value<DWORD>(HKEY_CURRENT_USER, testSubkey, stringValueName);
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
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value);
            const auto result = wil::reg::get_value<std::wstring>(hkey.get(), stringValueName);
            REQUIRE(result == value);
        }

        // reading the wrong type should fail
        try
        {
            wil::reg::get_value<DWORD>(hkey.get(), stringValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value/try_get_value_wstring: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_wstring(hkey.get(), stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value.c_str());
            result = wil::reg::try_get_value_wstring(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_wstring(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        try
        {
            wil::reg::try_get_value_dword(hkey.get(), stringValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value/try_get_value_wstring: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::try_get_value_wstring(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::try_get_value_wstring(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_wstring(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        try
        {
            wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, stringValueName);
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
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : stringTestArray)
        {
            WCHAR result[100]{};
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
        DWORD dwordValue{};
        hr = wil::reg::get_value_dword_nothrow(hkey.get(), stringValueName, &dwordValue);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_nothrow/get_value_string_nothrow: with string key")
    {
        for (const auto& value : stringTestArray)
        {
            WCHAR result[100]{};
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
        DWORD dwordValue{};
        hr = wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &dwordValue);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
}
#endif

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("BasicRegistryTests::expanded_wstring", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_expanded_string_nothrow/get_value_expanded_wstring_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[100]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, 100);
            REQUIRE(expanded_result != 0);
            REQUIRE(expanded_result < 100);

            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), stringValueName, value.c_str()));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_wstring_nothrow(hkey.get(), stringValueName, &result));
            REQUIRE(result == expanded_value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_wstring_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_expanded_wstring_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_dword_nothrow(hkey.get(), stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_expanded_string_nothrow/get_value_expanded_wstring_nothrow: with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[100]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, 100);
            REQUIRE(expanded_result != 0);
            REQUIRE(expanded_result < 100);

            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            std::wstring result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
            REQUIRE(result == expanded_value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_expanded_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        std::wstring result{};
        auto hr = wil::reg::get_value_expanded_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[100]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, 100);
            REQUIRE(expanded_result != 0);
            REQUIRE(expanded_result < 100);

            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(hkey.get(), stringValueName, value.c_str()));
            WCHAR result[100]{};
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
        WCHAR expanded_value[100]{};
        const auto expanded_result = ::ExpandEnvironmentStringsW(L"%WINDIR%", expanded_value, 100);
        REQUIRE(expanded_result != 0);
        REQUIRE(expanded_result < 100);
        REQUIRE(0 == wcscmp(valid_buffer_result, expanded_value));


        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_nothrow(hkey.get(), stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }
    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow: with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[100]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, 100);
            REQUIRE(expanded_result != 0);
            REQUIRE(expanded_result < 100);

            REQUIRE_SUCCEEDED(wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
            WCHAR result[100]{};
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

        WCHAR expanded_value[100]{};
        const auto expanded_result = ::ExpandEnvironmentStringsW(L"%WINDIR%", expanded_value, 100);
        REQUIRE(expanded_result != 0);
        REQUIRE(expanded_result < 100);
        REQUIRE(0 == wcscmp(valid_buffer_result, expanded_value));

        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    SECTION("set_value_expanded_string/get_value_expanded_wstring: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[100]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, 100);
            REQUIRE(expanded_result != 0);
            REQUIRE(expanded_result < 100);

            wil::reg::set_value_expanded_string(hkey.get(), stringValueName, value.c_str());
            auto result = wil::reg::get_value_expanded_wstring(hkey.get(), stringValueName);
            REQUIRE(result == expanded_value);

            // and verify default value name
            wil::reg::set_value_expanded_string(hkey.get(), nullptr, value.c_str());
            result = wil::reg::get_value_expanded_wstring(hkey.get(), nullptr);
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_expanded_wstring(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        try
        {
            wil::reg::get_value_dword(hkey.get(), stringValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
    SECTION("set_value_expanded_string/get_value_expanded_wstring: with string key")
    {
        for (const auto& value : expandedStringTestArray)
        {
            // verify the expanded string
            WCHAR expanded_value[100]{};
            const auto expanded_result = ::ExpandEnvironmentStringsW(value.c_str(), expanded_value, 100);
            REQUIRE(expanded_result != 0);
            REQUIRE(expanded_result < 100);

            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::get_value_expanded_wstring(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == expanded_value);

            // and verify default value name
            wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            result = wil::reg::get_value_expanded_wstring(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == expanded_value);
        }

        // fail get* if the value doesn't exist
        try
        {
            wil::reg::get_value_expanded_wstring(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }

        // fail if get* requests the wrong type
        try
        {
            wil::reg::get_value_dword(HKEY_CURRENT_USER, testSubkey, stringValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }
}
#endif

#ifdef __WIL_WINREG_STL
TEST_CASE("BasicRegistryTests::multi-strings", "[registry][get_registry_string]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_multistring_nothrow/get_value_multistring_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : multiStringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(hkey.get(), stringValueName, value));
            std::vector<std::wstring> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(hkey.get(), stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(hkey.get(), nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::vector<std::wstring> result{};
        auto hr = wil::reg::get_value_multistring_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_dword_nothrow(hkey.get(), stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    SECTION("set_value_multistring_nothrow/get_value_multistring_nothrow: with string key")
    {
        for (const auto& value : multiStringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
            std::vector<std::wstring> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::vector<std::wstring> result{};
        auto hr = wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    SECTION("set_value_nothrow/get_value_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : multiStringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, value));
            std::vector<std::wstring> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::vector<std::wstring> result{};
        auto hr = wil::reg::get_value_nothrow(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_nothrow(hkey.get(), stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    SECTION("set_value_nothrow/get_value_nothrow: with string key")
    {
        for (const auto& value : multiStringTestArray)
        {
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
            std::vector<std::wstring> result{};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::vector<std::wstring> result{};
        auto hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str(), &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        DWORD dwordResult{};
        hr = wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &dwordResult);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("set_value_multistring/get_value_multistring: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : multiStringTestArray)
        {
            wil::reg::set_value_multistring(hkey.get(), stringValueName, value);
            auto result = wil::reg::get_value_multistring(hkey.get(), stringValueName);
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_multistring(hkey.get(), nullptr, value);
            result = wil::reg::get_value_multistring(hkey.get(), nullptr);
            REQUIRE(result == value);
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
        try
        {
            wil::reg::get_value_dword(hkey.get(), stringValueName);
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
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            result = wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == value);
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
        try
        {
            wil::reg::get_value_dword(HKEY_CURRENT_USER, testSubkey, stringValueName);
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
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, &hkey, wil::reg::key_access::readwrite));

        for (const auto& value : multiStringTestArray)
        {
            wil::reg::set_value(hkey.get(), stringValueName, value);
            auto result = wil::reg::try_get_value_multistring(hkey.get(), stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value(hkey.get(), nullptr, value);
            result = wil::reg::try_get_value_multistring(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_multistring(hkey.get(), (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        try
        {
            wil::reg::try_get_value_dword(hkey.get(), stringValueName);
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
            REQUIRE(result == value);

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            result = wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, (std::wstring(stringValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        try
        {
            wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, stringValueName);
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