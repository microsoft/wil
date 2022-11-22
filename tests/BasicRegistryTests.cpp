
#include <string>
#if _HAS_CXX17
#include <optional>
#endif // _HAS_CXX17
#include <wil/filesystem.h>
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
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

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

#if defined(_OPTIONAL_)
    SECTION("get optional with string key and value name")
    {
        const auto emptyResult = wil::try_get_registry_dword(HKEY_CURRENT_USER, testSubkey, L"NonExistentKey");
        REQUIRE(emptyResult == std::nullopt);

        for (DWORD&& value : { 4, 1, 0 })
        {
            wil::set_registry_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName, value);
            const auto result = wil::try_get_registry_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }
    }
#endif // defined(_OPTIONAL_)
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

#if defined(_OPTIONAL_)
    SECTION("get optional with string key and DEFAULT value name")
    {
        const auto emptyResult = wil::try_get_registry_dword(HKEY_CURRENT_USER, testSubkey, nullptr);
        REQUIRE(emptyResult == std::nullopt);

        for (DWORD&& value : { 4, 1, 0 })
        {
            wil::set_registry_dword(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            const auto result = wil::try_get_registry_dword(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }
    }
#endif // defined(_OPTIONAL_)
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

    SECTION("get and set with string key and DEFAULT value, nothrowish")
    {
        // Evil embedded null
        std::wstring value = L"something \0 pithy";
        REQUIRE_SUCCEEDED(wil::set_registry_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, value));
        auto result = wil::get_registry_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == value);
    }

    SECTION("get and set with string key and value name")
    {
        for (std::wstring&& value : { L"No no no", L"", L"stick to the stuff you know", L"better \0 by far", L"\0"})
        {
            wil::set_registry_string(HKEY_CURRENT_USER, testSubkey, stringValueName, value);
            const auto result = wil::get_registry_string(HKEY_CURRENT_USER, testSubkey, stringValueName);
            REQUIRE(result == value);
        }
    }

    SECTION("get and set with string key and DEFAULT value name")
    {
        for (std::wstring&& value : { L"buffalo buffalo", L"", L"buffalo buffalo buffalo", L"buffalo \0 buffalo buffalo buffalo", L"\0"})
        {
            wil::set_registry_string(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            const auto result = wil::get_registry_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result == value);
        }
    }

#if defined(_OPTIONAL_)
    SECTION("get optional with string key and DEFAULT value name")
    {
        const auto emptyResult = wil::try_get_registry_string(HKEY_CURRENT_USER, testSubkey, nullptr);
        REQUIRE(emptyResult == std::nullopt);

        for (std::wstring&& value : { L"Ah wretch!", L"", L"said they, the bird to slay", L"\0", L"That made the breeze \0 to blow!"})
        {
            wil::set_registry_string(HKEY_CURRENT_USER, testSubkey, nullptr, value);
            const auto result = wil::try_get_registry_string(HKEY_CURRENT_USER, testSubkey, nullptr);
            REQUIRE(result.has_value());
            REQUIRE(result == value);
        }
    }
#endif // defined(_OPTIONAL_)
}
#endif