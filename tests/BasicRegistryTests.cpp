
#include <wil/filesystem.h>
#include <string>
#include <wil/registry.h>
#include <wil/resource.h>

#include <memory> // For shared_event_watcher
#include <wil/resource.h>

#include "common.h"

constexpr auto testSubkey = L"Software\\Microsoft\\BasicRegistryTest";
constexpr auto dwordValueName = L"MyDwordValue";
constexpr auto stringValueName = L"MyStringValue";

TEST_CASE("BasicRegistryTests::Dwords", "[registry][get_registry_dword]")
{
    SECTION("get and set with string key and value name, nothrow")
    {
        DWORD value = 4;
        REQUIRE_SUCCEEDED(wil::set_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, value));

        DWORD result{ 0 };
        REQUIRE_SUCCEEDED(wil::get_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result));
        REQUIRE(result == value);

        value = 1;
        REQUIRE_SUCCEEDED(wil::set_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, value));
        REQUIRE_SUCCEEDED(wil::get_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result));
        REQUIRE(result == value);

        value = 0;
        REQUIRE_SUCCEEDED(wil::set_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, value));
        REQUIRE_SUCCEEDED(wil::get_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result));
        REQUIRE(result == value);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("get and set with string key and value name")
    {
        for (DWORD&& value : { 4, 1, 0 })
        {
            wil::set_registry_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName, value);
            const auto result = wil::get_registry_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            REQUIRE(result == value);
        }
    }
#endif

    SECTION("get and set with string key and DEFAULT value name, nothrow")
    {
        DWORD value = 4;
        REQUIRE_SUCCEEDED(wil::set_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));

        DWORD result{ 0 };
        REQUIRE_SUCCEEDED(wil::get_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
        REQUIRE(result == value);

        value = 1;
        REQUIRE_SUCCEEDED(wil::set_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
        REQUIRE_SUCCEEDED(wil::get_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
        REQUIRE(result == value);

        value = 0;
        REQUIRE_SUCCEEDED(wil::set_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
        REQUIRE_SUCCEEDED(wil::get_registry_dword_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
        REQUIRE(result == value);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("get and set with string key and DEFAULT value name")
    {
        for (DWORD&& value : { 4, 1, 0 })
        {
            wil::set_registry_dword(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            const auto result = wil::get_registry_dword(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == value);
        }
    }
#endif
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("BasicRegistryTests::Strings", "[registry][get_registry_string]")
{
    SECTION("get and set with string key and value name, nothrowish")
    {
        std::wstring value{ L"Hello there!" };
        REQUIRE_SUCCEEDED(wil::set_registry_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
        auto result = wil::get_registry_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == value);

        value = L"It's over, Anakin!";
        REQUIRE_SUCCEEDED(wil::set_registry_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
        result = wil::get_registry_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == value);

        value = L"";
        REQUIRE_SUCCEEDED(wil::set_registry_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
        result = wil::get_registry_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == value);

        // Evil embedded null
        value = L"I have the \0 high ground";
        REQUIRE_SUCCEEDED(wil::set_registry_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
        result = wil::get_registry_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == value);
    }

    SECTION("get and set with string key and value name")
    {
        for (std::wstring&& value : { L"No no no", L"", L"stick to the stuff you know", L"better \0 by far" })
        {
            wil::set_registry_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value);
            const auto result = wil::get_registry_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == value);
        }
    }
}
#endif