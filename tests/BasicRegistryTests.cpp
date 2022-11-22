
#include <wil/filesystem.h>
#include <wil/registry.h>
#include <wil/resource.h>

#include <memory> // For shared_event_watcher
#include <wil/resource.h>

#include "common.h"

constexpr auto testSubkey = L"Software\\Microsoft\\RegistryWatcherTest";
constexpr auto dwordValueName = L"MyDwordValue";

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