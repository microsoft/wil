#include <memory> // For shared_event_watcher
#include <string>
#include <wil/filesystem.h>
#include <wil/registry_basic.h>
#include <wil/resource.h>

#include "common.h"

constexpr auto* testSubkey = L"Software\\Microsoft\\BasicRegistryTest";
constexpr auto* dwordValueName = L"MyDwordValue";
constexpr auto* stringValueName = L"MyStringValue";

constexpr DWORD dwordTestArray[] = { DWORD(-1), 1, 0 };
const std::wstring stringTestArray[] = { L".", L"", L"Hello there!", L"\0" };

TEST_CASE("BasicRegistryTests::Dwords", "[registry][get_registry_dword]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_dword_nothrow/get_value_dword_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

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
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

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
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

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
            wil::reg::get_value_string(hkey.get(), dwordValueName);
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
            wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            // should throw
            REQUIRE_FALSE(true);
        }
        catch (const wil::ResultException& e)
        {
            REQUIRE(e.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        }
    }

    /*
    SECTION("get and set with string key and value name, template versions")
    {
        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, dwordValueName, value);
            const auto result = wil::reg::get_value(HKEY_CURRENT_USER, testSubkey, dwordValueName, 2);
            REQUIRE(DWORD(result) == value);
        }
    }
    */

    SECTION("set_value_dword/try_get_value_dword: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        const auto emptyResult = wil::reg::try_get_value_dword(hkey.get(), L"NonExistentKey");
        REQUIRE(!emptyResult.has_value());
        REQUIRE(!emptyResult);

        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value_dword(hkey.get(), dwordValueName, value);
            auto result = wil::reg::try_get_value_dword(hkey.get(), dwordValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result.value == value);

            // and verify default value name
            wil::reg::set_value_dword(hkey.get(), nullptr, value);
            result = wil::reg::try_get_value_dword(hkey.get(), nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result.value == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_dword(hkey.get(), (std::wstring(dwordValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());
        REQUIRE(result.inner_error == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
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
    SECTION("set_value_dword/try_get_value_dword: with string key")
    {
        const auto emptyResult = wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, L"NonExistentKey");
        REQUIRE(!emptyResult.has_value());
        REQUIRE(!emptyResult);

        for (const auto& value : dwordTestArray)
        {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName, value);
            auto result = wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result.value == value);

            // and verify default value name
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            result = wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result.value == value);
        }

        // fail get* if the value doesn't exist
        const auto result = wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, (std::wstring(dwordValueName) + L"_not_valid").c_str());
        REQUIRE(!result);
        REQUIRE(!result.has_value());
        REQUIRE(result.inner_error == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
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
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("BasicRegistryTests::Strings", "[registry][get_registry_string]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    // TODO: no subkey version, too.
    SECTION("get and set with string key and value name, nothrow")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            auto result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == value);
        }
    }

    SECTION("get and set with string key and DEFAULT value, nothrow")
    {
        std::wstring value = L"something pithy";
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
        auto result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == value);
    }

    SECTION("get and set with string key and value name")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            const auto result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == value);
        }
    }

    SECTION("try_get and set with string key and value name")
    {
        const auto emptyResult = wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(!emptyResult.has_value());
        REQUIRE(!emptyResult);

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            const auto result = wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result.value == value);
            REQUIRE(result.value_or(L"") == value);
        }
    }

    SECTION("get and set with string key and DEFAULT value name")
    {
        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            const auto result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == value);
        }
    }

    SECTION("try_get values with string key and DEFAULT value name")
    {
        const auto emptyResult = wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, nullptr);
        REQUIRE(!emptyResult.has_value());
        REQUIRE(!emptyResult);

        for (const auto& value : stringTestArray)
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            const auto result = wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result.has_value());
            REQUIRE(result.value == value);
            REQUIRE(result.value_or(L"") == value);
        }
    }
}
#endif

#ifdef __WIL_WINREG_STL
TEST_CASE("BasicRegistryTests::MultiStrings", "[registry][get_registry_multi_string]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    // TODO: no subkey version, too.
    SECTION("get and set with multi-strings key and value name, nothrow")
    {
        std::vector<std::wstring> result;
        std::vector<std::wstring> value{ {L"Hello there!"}, {L"Hello a second time!"}, {L"Hello a third time!"} };
        REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
        result.push_back(L"Garbage");
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result == value);

        // in this case, we are writing 4 null terminator characters
        // per rules for REG_MULTI_SZ, the 'end' is indicated by 2 successive null characters
        // thus per registry rules, this should return zero strings       
        value = { {L""}, {L""}, {L""} };
        REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
        result.push_back(L"Garbage");
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result == std::vector<std::wstring>{}); // should return zero strings

        value = {};
        REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
        result.push_back(L"Garbage");
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result == value);

        value = { {L"a"} };
        REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
        result.push_back(L"Garbage");
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_wstring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result == value);
    }

    SECTION("get and set with string key and DEFAULT value, nothrow")
    {
        std::wstring value = L"something pithy";
        REQUIRE_SUCCEEDED(wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str()));
        auto result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == value);
    }

    SECTION("get and set with string key and value name")
    {
        for (std::wstring&& value : { L".", L"", L"buffalo buffalo buffalo", L"\0" })
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value.c_str());
            const auto result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == value);
        }
    }

    SECTION("get and set with string key and DEFAULT value name")
    {
        for (std::wstring&& value : { L".", L"", L"buffalo buffalo buffalo", L"\0" })
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            const auto result = wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == value);
        }
    }
#ifdef WIL_ENABLE_EXCEPTIONS
#endif

    SECTION("get optional with string key and DEFAULT value name")
    {
        const auto emptyResult = wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, nullptr);
        REQUIRE(!emptyResult.has_value());
        REQUIRE(!emptyResult);

        for (std::wstring&& value : { L".", L"", L"buffalo buffalo buffalo", L"\0" })
        {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, nullptr, value.c_str());
            const auto result = wil::reg::try_get_value_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result);
            REQUIRE(result.has_value());
            REQUIRE(result.value == value);
        }
    }
}
#endif