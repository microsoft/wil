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
constexpr auto* multiStringValueName = L"MyMultiStringValue";
constexpr auto* binaryValueName = L"MyBinaryValue";
constexpr auto* invalidValueName = L"NonExistentValue";
constexpr auto* wrongTypeValueName = L"InvalidTypeValue";

constexpr uint32_t test_dword_two = 2ul;
constexpr uint32_t test_dword_three = 3ul;
constexpr uint32_t test_dword_zero = 0ul;
constexpr uint64_t test_qword_zero = 0ull;
const std::wstring test_string_empty{};

// The empty multistring array has specific behavior: it will be read as an array with one string.
const std::vector<std::wstring> test_multistring_empty{};

constexpr uint32_t test_expanded_string_buffer_size = 100;

constexpr std::array<DWORD, 3> dwordTestArray = { static_cast<DWORD>(-1), 1, 0 };
const std::vector<DWORD> dwordTestVector = { static_cast<DWORD>(-1), 1, 0 };
constexpr std::array<DWORD64, 3> qwordTestArray = { static_cast<DWORD64>(-1), 1, 0 };
const std::vector<DWORD64> qwordTestVector = { static_cast<DWORD64>(-1), 1, 0 };
const std::array<std::wstring, 4> stringTestArray = { L".", L"", L"Hello there!", L"\0" };
const std::wstring expandedStringTestArray[] = { L".", L"", L"%WINDIR%", L"\0" };
const std::array<std::vector<std::wstring>, 6> multiStringTestArray{
    std::vector<std::wstring>{ {} },
    std::vector<std::wstring>{ {}, {} },
    std::vector<std::wstring>{ {}, {L"."}, {}, {L"."}, {}, {} },
    std::vector<std::wstring>{ {L"Hello there!"}, {L"Hello a second time!"}, {L"Hello a third time!"} },
    std::vector<std::wstring>{ {L""}, {L""}, {L""} },
    std::vector<std::wstring>{ {L"a"} }
};
const std::vector<std::vector<std::wstring>> multiStringTestVector{
    std::vector<std::wstring>{ {} },
    std::vector<std::wstring>{ {}, {} },
    std::vector<std::wstring>{ {}, {L"."}, {}, {L"."}, {}, {} },
    std::vector<std::wstring>{ {L"Hello there!"}, {L"Hello a second time!"}, {L"Hello a third time!"} },
    std::vector<std::wstring>{ {L""}, {L""}, {L""} },
    std::vector<std::wstring>{ {L"a"} }
};

const std::vector<BYTE> emptyStringTestValue{};
const std::vector<BYTE> nonNullTerminatedString{ {'a'}, {0}, {'b'}, {0}, {'c'}, {0}, {'d'}, {0}, {'e'}, {0}, {'f'}, {0}, {'g'}, {0}, {'h'}, {0}, {'i'}, {0}, {'j'}, {0}, {'k'}, {0}, {'l'}, {0} };
const std::wstring nonNullTerminatedStringFixed{ L"abcdefghijkl" };

const std::array<std::vector<BYTE>, 3> vectorBytesTestArray = {
    std::vector<BYTE>{ {0x00} },
    std::vector<BYTE>{},
    std::vector<BYTE>{ {0x1}, {0x2}, {0x3}, {0x4}, {0x5}, {0x6},{0x7}, {0x8}, {0x9},{0xa}, {0xb}, {0xc}, {0xd}, {0xe}, {0xf} },
};

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

#if defined WIL_ENABLE_EXCEPTIONS
void VerifyThrowsHr(HRESULT hr, std::function<void()> fn)
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
            {
                wil::unique_hkey invalid_key{ wil::reg::open_unique_key(hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite) };
            });
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
            {
                wil::unique_hkey invalid_key{ wil::reg::open_unique_key(HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite) };
            });
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
            {
                wil::shared_hkey invalid_key{ wil::reg::open_shared_key(hkey.get(), (std::wstring(subSubKey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite) };
            });
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
            {
                wil::shared_hkey invalid_key{ wil::reg::open_shared_key(HKEY_CURRENT_USER, (std::wstring(testSubkey) + L"_not_valid").c_str(), wil::reg::key_access::readwrite) };
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
    template<typename T>
    struct type_identity
    {
        using type = T;
    };

    template<typename GetOutputT, typename SetT, typename GetInputT, typename GetT = GetInputT>
    void verify_set_nothrow(
        PCWSTR valueName,
        GetInputT value,
        std::function<HRESULT(PCWSTR, GetOutputT)> getFn,
        std::function<HRESULT(PCWSTR, SetT)> setFn)
    {
        REQUIRE_SUCCEEDED(setFn(valueName, value));
        GetT result{};
        REQUIRE_SUCCEEDED(getFn(valueName, &result));
        REQUIRE(result == value);

        // and verify default value name
        REQUIRE_SUCCEEDED(setFn(nullptr, value));
        result = {};
        REQUIRE_SUCCEEDED(getFn(nullptr, &result));
        REQUIRE(result == value);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    template<typename GetOutputT, typename SetT, typename GetInputT = GetOutputT>
    void verify_good(
        PCWSTR valueName,
        GetInputT value,
        std::function<GetOutputT(PCWSTR)> getFn,
        std::function<void(PCWSTR, SetT)> setFn)
    {
        setFn(valueName, value);
        auto result = getFn(valueName);
        REQUIRE(result == value);

        // and verify default value name
        setFn(nullptr, value);
        result = getFn(nullptr);
        REQUIRE(result == value);
    }
#endif

    template<typename GetOutputT, typename GetT>
    void verify_not_exist_nothrow(
        std::function<HRESULT(PCWSTR, GetOutputT)> getFn)
    {
        // fail get* if the value doesn't exist
        GetT result{};
        const HRESULT hr = getFn(invalidValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    };

#ifdef WIL_ENABLE_EXCEPTIONS
    template<typename GetOutputT>
    void verify_not_exist(std::function<GetOutputT(PCWSTR)> getFn)
    {
        // fail get* if the value doesn't exist
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
            {
                const auto ignored = getFn(invalidValueName);
                ignored;
            });
    };
#endif

    template<typename GetT, typename WrongSetT>
    void verify_wrong_type_nothrow(
        PCWSTR valueName,
        WrongSetT value,
        std::function<HRESULT(PCWSTR, std::add_pointer_t<GetT>)> getFn,
        std::function<HRESULT(PCWSTR, typename type_identity<WrongSetT>::type)> setFn)
    {
        REQUIRE_SUCCEEDED(setFn(valueName, value));
        GetT result{};
        const HRESULT hr = getFn(valueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    template<typename GetT, typename WrongSetT>
    void verify_wrong_type(
        PCWSTR valueName,
        WrongSetT value,
        std::function<GetT(PCWSTR)> getFn,
        std::function<void(PCWSTR, typename type_identity<WrongSetT>::type)> setFn)
    {
        // fail if get* requests the wrong type
        setFn(valueName, value);
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
            {
                const auto ignored = getFn(valueName);
                ignored;
            });
    }
#endif

    template<typename OutT, typename SetT = OutT, size_t N>
    void verify_get_set(
        const std::array<SetT, N>& testArray,
        PCWSTR valueName,
        std::function<HRESULT(PCWSTR, typename type_identity<OutT>::type*)> getFn,
        std::function<HRESULT(PCWSTR, const typename type_identity<SetT>::type&)> setFn,
        std::function<HRESULT(PCWSTR)> const& setWrongTypeFn)
    {
        for (const auto& value : testArray)
        {
            REQUIRE_SUCCEEDED(setFn(valueName, value));
            OutT result{};
            REQUIRE_SUCCEEDED(getFn(valueName, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(setFn(nullptr, value));
            result = {};
            REQUIRE_SUCCEEDED(getFn(nullptr, &result));
            REQUIRE(result == value);
        }
        
        // fail get* if the value doesn't exist
        SetT result{};
        HRESULT hr = getFn(invalidValueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // TODO: support array
        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(setWrongTypeFn(valueName));
        hr = getFn(valueName, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    template<typename OutT, typename SetT = OutT, size_t N>
    void verify_get_set(
        const wil::unique_hkey& hkey,
        const std::array<SetT, N>& testArray,
        PCWSTR valueName,
        std::function<HRESULT(PCWSTR)> setWrongTypeFn)
    {
        verify_get_set<OutT>(
            testArray,
            valueName,
            [&hkey](PCWSTR valueName, OutT* output) -> HRESULT { return wil::reg::get_value_nothrow(hkey.get(), valueName, output); },
            [&hkey](PCWSTR valueName, const SetT& input) { return wil::reg::set_value_nothrow(hkey.get(), valueName, input); },
            setWrongTypeFn);
    }

    template<typename OutT, typename SetT = OutT, size_t N>
    void verify_get_set(
        HKEY hkey,
        PCWSTR subkey,
        const std::array<SetT, N>& testArray,
        PCWSTR valueName,
        std::function<HRESULT(PCWSTR)> setWrongTypeFn)
    {
        verify_get_set<OutT>(
            testArray,
            valueName,
            [&hkey, &subkey](PCWSTR valueName, OutT* output) -> HRESULT { return wil::reg::get_value_nothrow(hkey, subkey, valueName, output); },
            [&hkey, &subkey](PCWSTR valueName, const SetT& input) { return wil::reg::set_value_nothrow(hkey, subkey, valueName, input); },
            setWrongTypeFn);
    }
}

TEST_CASE("BasicRegistryTests::ReadWrite", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("get and set nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // DWORDs
        verify_get_set<DWORD>(
            hkey,
            dwordTestArray,
            dwordValueName,
            [&hkey](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_nothrow(hkey.get(), valueName, test_qword_zero); });

        // QWORDs
        verify_get_set<DWORD64>(
            hkey,
            qwordTestArray,
            qwordValueName,
            [&hkey](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_nothrow(hkey.get(), valueName, test_dword_zero); });

#ifdef WIL_ENABLE_EXCEPTIONS
        // TODO: strings shouldn't require exceptions, right?
        // TODO: multiple string types
        verify_get_set<std::wstring>(
            hkey,
            stringTestArray,
            stringValueName,
            [&hkey](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_nothrow(hkey.get(), valueName, test_dword_zero); });
        // TODO:
        //TestWrongTypeFn(multiStringValueName, test_multistring_empty, test_string_empty);

        verify_get_set<std::vector<std::wstring>>(
            hkey,
            multiStringTestArray,
            multiStringValueName,
            [&hkey](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_nothrow(hkey.get(), valueName, test_dword_zero); });
        // TODO:
        //TestWrongTypeFn(stringValueName, test_string_empty, test_multistring_empty);

        // TODO: byte vectors
#endif
    }

    SECTION("get and set nothrow: with string key")
    {
        // DWORDs
        verify_get_set<DWORD>(
            HKEY_CURRENT_USER,
            testSubkey,
            dwordTestArray,
            dwordValueName,
            [](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, test_qword_zero); });

        // QWORDs
        verify_get_set<DWORD64>(
            HKEY_CURRENT_USER,
            testSubkey,
            qwordTestArray,
            qwordValueName,
            [](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });

#ifdef WIL_ENABLE_EXCEPTIONS
        // TODO: strings shouldn't require exceptions, right?
        // TODO: multiple string types
        verify_get_set<std::wstring>(
            HKEY_CURRENT_USER,
            testSubkey,
            stringTestArray,
            stringValueName,
            [](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });
        // TODO:
        //TestWrongTypeFn(multiStringValueName, test_multistring_empty, test_string_empty);

        verify_get_set<std::vector<std::wstring>>(
            HKEY_CURRENT_USER,
            testSubkey,
            multiStringTestArray,
            multiStringValueName,
            [](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });
        // TODO:
        //TestWrongTypeFn(stringValueName, test_string_empty, test_multistring_empty);

        // TODO: byte vectors
#endif
    }


#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("get, try_get, and set: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        const auto TestGoodFn = [&hkey](PCWSTR valueName, auto value)
        {
            wil::reg::set_value(hkey.get(), valueName, value);
            const auto result = wil::reg::get_value<decltype(value)>(hkey.get(), valueName);
            REQUIRE(result == value);

#if defined(__cpp_lib_optional)
            const auto optional_result = wil::reg::try_get_value<decltype(value)>(hkey.get(), valueName);
            REQUIRE(optional_result == value);
#endif

            // and verify default value name
            wil::reg::set_value(hkey.get(), valueName, value);
            const auto result2 = wil::reg::get_value<decltype(value)>(hkey.get(), valueName);
            REQUIRE(result2 == value);

#if defined(__cpp_lib_optional)
            const auto optional_result2 = wil::reg::try_get_value<decltype(value)>(hkey.get(), valueName);
            REQUIRE(optional_result2 == value);
#endif
        };

        const auto TestNonExistentFn = [&hkey](auto value)
        {
            // fail get* if the value doesn't exist
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
                {
                    const auto ignored = wil::reg::get_value<decltype(value)>(hkey.get(), invalidValueName);
                    ignored;
                });

#if defined(__cpp_lib_optional)
            // try_get should simply return nullopt
            const auto optional_result = wil::reg::try_get_value<decltype(value)>(hkey.get(), invalidValueName);
            REQUIRE(optional_result == std::nullopt);
#endif
        };

        const auto TestWrongTypeFn = [&hkey](PCWSTR valueName, auto originalValue, auto fetchedValue)
        {
            // reading the wrong type should fail
            wil::reg::set_value(hkey.get(), valueName, originalValue);
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
                {
                    const auto ignored = wil::reg::get_value<decltype(fetchedValue)>(hkey.get(), valueName);
                    ignored;
                });

#if defined(__cpp_lib_optional)
            // Same for try_get
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
                {
                    const auto ignored = wil::reg::try_get_value<decltype(fetchedValue)>(hkey.get(), valueName);
                    ignored;
                });
#endif
        };

        // DWORDs
        for (const auto& value : dwordTestArray)
        {
            TestGoodFn(dwordValueName, value);
        }
        TestNonExistentFn(test_dword_zero);
        TestWrongTypeFn(qwordValueName, test_qword_zero, test_dword_zero);

        // QWORDs
        for (const auto& value : qwordTestArray)
        {
            TestGoodFn(qwordValueName, value);
        }
        TestNonExistentFn(test_qword_zero);
        TestWrongTypeFn(dwordValueName, test_dword_zero, test_qword_zero);

        // TODO: strings shouldn't require exceptions, right?
        // TODO: multiple string types
        for (const auto& value : stringTestArray)
        {
            // TODO: avoid string cast
            TestGoodFn(stringValueName, std::wstring(value));
        }
        TestNonExistentFn(test_string_empty);
        TestWrongTypeFn(dwordValueName, test_dword_zero, test_string_empty);
        TestWrongTypeFn(multiStringValueName, test_multistring_empty, test_string_empty);

        // TODO: besto
        //for (const auto& value : multiStringTestArray)
        //{
        //    TestGoodFn(multiStringValueName, value);
        //}
        //TestNonExistentFn(test_multistring_empty);
        //TestWrongTypeFn(dwordValueName, test_dword_zero, test_multistring_empty);
        //TestWrongTypeFn(stringValueName, test_string_empty, test_multistring_empty);

        // TODO: byte vectors
    }

    SECTION("get and set: with string key")
    {
        const auto TestGoodFn = [](PCWSTR valueName, auto value)
        {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, valueName, value);
            const auto result = wil::reg::get_value<decltype(value)>(HKEY_CURRENT_USER, testSubkey, valueName);
            REQUIRE(result == value);

#if defined(__cpp_lib_optional)
            const auto optional_result = wil::reg::try_get_value<decltype(value)>(HKEY_CURRENT_USER, testSubkey, valueName);
            REQUIRE(optional_result == value);
#endif

            // and verify default value name
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, valueName, value);
            const auto result2 = wil::reg::get_value<decltype(value)>(HKEY_CURRENT_USER, testSubkey, valueName);
            REQUIRE(result2 == value);

#if defined(__cpp_lib_optional)
            const auto optional_result2 = wil::reg::try_get_value<decltype(value)>(HKEY_CURRENT_USER, testSubkey, valueName);
            REQUIRE(optional_result2 == value);
#endif
        };

        const auto TestNonExistentFn = [](auto value)
        {
            // fail get* if the value doesn't exist
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
                {
                    const auto ignored = wil::reg::get_value<decltype(value)>(HKEY_CURRENT_USER, testSubkey, invalidValueName);
                    ignored;
                });

#if defined(__cpp_lib_optional)
            // try_get should simply return nullopt
            const auto optional_result = wil::reg::try_get_value<decltype(value)>(HKEY_CURRENT_USER, testSubkey, invalidValueName);
            REQUIRE(optional_result == std::nullopt);
#endif
        };

        const auto TestWrongTypeFn = [](PCWSTR valueName, auto originalValue, auto fetchedValue)
        {
            // reading the wrong type should fail
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, valueName, originalValue);
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
                {
                    const auto ignored = wil::reg::get_value<decltype(fetchedValue)>(HKEY_CURRENT_USER, testSubkey, valueName);
                    ignored;
                });

#if defined(__cpp_lib_optional)
            // Same for try_get
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
                {
                    const auto ignored = wil::reg::try_get_value<decltype(fetchedValue)>(HKEY_CURRENT_USER, testSubkey, valueName);
                    ignored;
                });
#endif
        };

        // DWORDs
        for (const auto& value : dwordTestArray)
        {
            TestGoodFn(dwordValueName, value);
        }
        TestNonExistentFn(test_dword_zero);
        TestWrongTypeFn(qwordValueName, test_qword_zero, test_dword_zero);

        // QWORDs
        for (const auto& value : qwordTestArray)
        {
            TestGoodFn(qwordValueName, value);
        }
        TestNonExistentFn(test_qword_zero);
        TestWrongTypeFn(dwordValueName, test_dword_zero, test_qword_zero);

        // TODO: strings shouldn't require exceptions, right?
        // TODO: multiple string types
        for (const auto& value : stringTestArray)
        {
            // TODO: avoid string cast
            TestGoodFn(stringValueName, std::wstring(value));
        }
        TestNonExistentFn(test_string_empty);
        TestWrongTypeFn(dwordValueName, test_dword_zero, test_string_empty);
        TestWrongTypeFn(multiStringValueName, test_multistring_empty, test_string_empty);

        // TODO: besto
        //for (const auto& value : multiStringTestArray)
        //{
        //    TestGoodFn(multiStringValueName, value);
        //}
        //TestNonExistentFn(test_multistring_empty);
        //TestWrongTypeFn(dwordValueName, test_dword_zero, test_multistring_empty);
        //TestWrongTypeFn(stringValueName, test_string_empty, test_multistring_empty);

        // TODO: byte vectors
    }
#endif

    SECTION("typed get and set nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        const auto setQwordFn = [&hkey](PCWSTR valueName, uint64_t input) -> HRESULT { return wil::reg::set_value_qword_nothrow(hkey.get(), valueName, input); };

        // DWORDs
        const auto getDwordFn = [&hkey](PCWSTR valueName, DWORD* output) -> HRESULT { return wil::reg::get_value_dword_nothrow(hkey.get(), valueName, output); };
        const auto setDwordFn = [&hkey](PCWSTR valueName, uint32_t input) -> HRESULT { return wil::reg::set_value_dword_nothrow(hkey.get(), valueName, input); };
        for (const auto& value : dwordTestArray)
        {
            verify_set_nothrow<DWORD*, uint32_t>(
                dwordValueName,
                value,
                getDwordFn,
                setDwordFn);
        }
        verify_not_exist_nothrow<DWORD*, DWORD>(getDwordFn);
        verify_wrong_type_nothrow<DWORD>(
            qwordValueName,
            test_qword_zero,
            getDwordFn,
            setQwordFn);

        // QWORDs
        const auto getQwordFn = [&hkey](PCWSTR valueName, DWORD64* output) -> HRESULT { return wil::reg::get_value_qword_nothrow(hkey.get(), valueName, output); };
        for (const auto& value : qwordTestArray)
        {
            verify_set_nothrow<DWORD64*, uint64_t>(
                qwordValueName,
                value,
                getQwordFn,
                setQwordFn);
        }
        verify_not_exist_nothrow<DWORD64*, DWORD64>(getQwordFn);
        verify_wrong_type_nothrow<DWORD64>(
            dwordValueName,
            test_dword_zero,
            getQwordFn,
            setDwordFn);

#ifdef WIL_ENABLE_EXCEPTIONS
        // TODO: strings shouldn't require exceptions, right?
        // TODO: multiple string types

        const auto setMultistringFn = [&hkey](PCWSTR valueName, std::vector<std::wstring> const& input) -> HRESULT { return wil::reg::set_value_multistring_nothrow(hkey.get(), valueName, input); };

        const auto getStringFn = [&hkey](PCWSTR valueName, std::wstring* output) -> HRESULT {
            // TODO: should we take a reference or a pointer?
            return wil::reg::get_value_string_nothrow(hkey.get(), valueName, *output);
        };
        const auto setStringFn = [&hkey](PCWSTR valueName, PCWSTR input) -> HRESULT { return wil::reg::set_value_string_nothrow(hkey.get(), valueName, input); };
        for (const auto& value : stringTestArray)
        {
            verify_set_nothrow<std::wstring*, PCWSTR, PCWSTR, std::wstring>(
                stringValueName,
                value.c_str(),
                getStringFn,
                setStringFn);
        }
        verify_not_exist_nothrow<std::wstring*, std::wstring>(getStringFn);
        verify_wrong_type_nothrow<std::wstring>(
            dwordValueName,
            test_dword_zero,
            getStringFn,
            setDwordFn);
        verify_wrong_type_nothrow<std::wstring>(
            multiStringValueName,
            test_multistring_empty,
            getStringFn,
            [&hkey](PCWSTR valueName, std::vector<std::wstring> const& input) -> HRESULT { return wil::reg::set_value_multistring_nothrow(hkey.get(), valueName, input); });

        const auto getMultistringFn = [&hkey](PCWSTR valueName, std::vector<std::wstring>* output) -> HRESULT { return wil::reg::get_value_multistring_nothrow(hkey.get(), valueName, output); };
        for (const auto& value : multiStringTestArray)
        {
            verify_set_nothrow<std::vector<std::wstring>*, std::vector<std::wstring>>(
                multiStringValueName,
                value,
                getMultistringFn,
                setMultistringFn);
        }
        verify_not_exist_nothrow<std::vector<std::wstring>*, std::vector<std::wstring>>(getMultistringFn);
        verify_wrong_type_nothrow<std::vector<std::wstring>>(
            dwordValueName,
            test_dword_zero,
            getMultistringFn,
            setDwordFn);
        // TODO: std::wstring should be const&
        verify_wrong_type_nothrow<std::vector<std::wstring>>(
            stringValueName,
            test_string_empty,
            getMultistringFn,
            [&hkey](PCWSTR valueName, std::wstring input) -> HRESULT { return wil::reg::set_value_string_nothrow(hkey.get(), valueName, input.c_str()); });

        // TODO: expanded strings
        // TODO: byte vectors
#endif
    }

    SECTION("typed get and set nothrow: with string key")
    {
        // DWORDs
        for (const auto& value : dwordTestArray)
        {
            verify_set_nothrow<DWORD*, uint32_t>(
                dwordValueName,
                value,
                [](PCWSTR valueName, DWORD* output) -> HRESULT { return wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); },
                [](PCWSTR valueName, uint32_t input) -> HRESULT { return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });
        }
        verify_not_exist_nothrow<DWORD*, DWORD>(
            [](PCWSTR valueName, DWORD* output) -> HRESULT { return wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); });
        verify_wrong_type_nothrow<DWORD>(
            qwordValueName,
            test_qword_zero,
            [](PCWSTR valueName, DWORD* output) -> HRESULT { return wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); },
            [](PCWSTR valueName, uint64_t input) -> HRESULT { return wil::reg::set_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });

        // QWORDs
        for (const auto& value : qwordTestArray)
        {
            verify_set_nothrow<DWORD64*, uint64_t>(
                qwordValueName,
                value,
                [](PCWSTR valueName, DWORD64* output) -> HRESULT { return wil::reg::get_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); },
                [](PCWSTR valueName, uint64_t input) -> HRESULT { return wil::reg::set_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });
        }
        verify_not_exist_nothrow<DWORD64*, DWORD64>(
            [](PCWSTR valueName, DWORD64* output) -> HRESULT { return wil::reg::get_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); });
        // TODO: std::wstring should be const&
        verify_wrong_type_nothrow<DWORD64>(
            dwordValueName,
            test_dword_zero,
            [](PCWSTR valueName, DWORD64* output) -> HRESULT { return wil::reg::get_value_qword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); },
            [](PCWSTR valueName, uint32_t input) -> HRESULT { return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });

#ifdef WIL_ENABLE_EXCEPTIONS
        // TODO: strings shouldn't require exceptions, right?
        // TODO: multiple string types
        for (const auto& value : stringTestArray)
        {
            verify_set_nothrow<std::wstring*, PCWSTR, PCWSTR, std::wstring>(
                stringValueName,
                value.c_str(),
                [](PCWSTR valueName, std::wstring* output) -> HRESULT {
                    // TODO: should we take a reference or a pointer?
                    return wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, *output);
                },
                [](PCWSTR valueName, PCWSTR input) -> HRESULT { return wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });
        }
        verify_not_exist_nothrow<std::wstring*, std::wstring>(
            [](PCWSTR valueName, std::wstring* output) -> HRESULT {
                // TODO: should we take a reference or a pointer?
                return wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, *output);
            });
        verify_wrong_type_nothrow<std::wstring>(
            dwordValueName,
            test_dword_zero,
            [](PCWSTR valueName, std::wstring* output) -> HRESULT {
                // TODO: should we take a reference or a pointer?
                return wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, *output);
            },
            [](PCWSTR valueName, uint32_t input) -> HRESULT { return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });
        verify_wrong_type_nothrow<std::wstring>(
            multiStringValueName,
            test_multistring_empty,
            [](PCWSTR valueName, std::wstring* output) -> HRESULT {
                // TODO: should we take a reference or a pointer?
                return wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, *output);
            },
            [](PCWSTR valueName, std::vector<std::wstring> const& input) -> HRESULT { return wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });

        for (const auto& value : multiStringTestArray)
        {
            verify_set_nothrow<std::vector<std::wstring>*, std::vector<std::wstring>>(
                multiStringValueName,
                value,
                [](PCWSTR valueName, std::vector<std::wstring>* output) -> HRESULT { return wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); },
                [](PCWSTR valueName, std::vector<std::wstring> const& input) -> HRESULT { return wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });
        }
        verify_not_exist_nothrow<std::vector<std::wstring>*, std::vector<std::wstring>>(
            [](PCWSTR valueName, std::vector<std::wstring>* output) -> HRESULT { return wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); });
        verify_wrong_type_nothrow<std::vector<std::wstring>>(
            dwordValueName,
            test_dword_zero,
            [](PCWSTR valueName, std::vector<std::wstring>* output) -> HRESULT { return wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); },
            [](PCWSTR valueName, uint32_t input) -> HRESULT { return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });
        // TODO: std::wstring should be const&
        verify_wrong_type_nothrow<std::vector<std::wstring>>(
            stringValueName,
            test_string_empty,
            [](PCWSTR valueName, std::vector<std::wstring>* output) -> HRESULT { return wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); },
            [](PCWSTR valueName, std::wstring input) -> HRESULT { return wil::reg::set_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input.c_str()); });

        // TODO: expanded strings
        // TODO: byte vectors
#endif
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("typed get and set: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // Lifted for testing wrong type set
        const auto qwordSetFn = [&hkey](PCWSTR valueName, uint64_t input) -> void { wil::reg::set_value_qword(hkey.get(), valueName, input); };

        // DWORDs
        const auto dwordGetFn = [&hkey](PCWSTR valueName) -> DWORD { return wil::reg::get_value_dword(hkey.get(), valueName); };
        const auto dwordSetFn = [&hkey](PCWSTR valueName, uint32_t input) -> void { wil::reg::set_value_dword(hkey.get(), valueName, input); };
        for (const auto& value : dwordTestArray)
        {
            verify_good<DWORD, uint32_t>(
                dwordValueName,
                value,
                dwordGetFn,
                dwordSetFn);
        }
        verify_not_exist<DWORD>(dwordGetFn);
        verify_wrong_type<DWORD, uint64_t>(qwordValueName, test_qword_zero, dwordGetFn, qwordSetFn);

        // QWORDs
        const auto qwordGetFn = [&hkey](PCWSTR valueName) -> DWORD64 { return wil::reg::get_value_qword(hkey.get(), valueName); };
        for (const auto& value : qwordTestArray)
        {
            verify_good<DWORD64, uint64_t>(
                qwordValueName,
                value,
                qwordGetFn,
                qwordSetFn);
        }
        verify_not_exist<DWORD64>(qwordGetFn);
        verify_wrong_type<DWORD64, uint32_t>(dwordValueName, test_dword_zero, qwordGetFn, dwordSetFn);

        // Elevated for testing wrong type set
        const auto multistringSetFn = [&hkey](PCWSTR valueName, std::vector<std::wstring> const& input) -> void { wil::reg::set_value_multistring(hkey.get(), valueName, input); };

        // TODO: multiple string types
        const auto stringGetFn = [&hkey](PCWSTR valueName) -> std::wstring { return wil::reg::get_value_string(hkey.get(), valueName); };
        const auto stringSetFn = [&hkey](PCWSTR valueName, PCWSTR input) -> void { wil::reg::set_value_string(hkey.get(), valueName, input); };
        for (const auto& value : stringTestArray)
        {
            verify_good<std::wstring, PCWSTR>(
                stringValueName,
                value.c_str(),
                stringGetFn,
                stringSetFn);
        }
        verify_not_exist<std::wstring>(stringGetFn);
        verify_wrong_type<std::wstring, uint32_t>(dwordValueName, test_dword_zero, stringGetFn, dwordSetFn);
        verify_wrong_type<std::wstring, std::vector<std::wstring>>(multiStringValueName, test_multistring_empty, stringGetFn, multistringSetFn);

        const auto multiStringGetFn = [&hkey](PCWSTR valueName) -> std::vector<std::wstring> { return wil::reg::get_value_multistring(hkey.get(), valueName); };
        for (const auto& value : multiStringTestArray)
        {
            verify_good<std::vector<std::wstring>, std::vector<std::wstring>>(
                multiStringValueName,
                value,
                multiStringGetFn,
                multistringSetFn);
        }
        verify_not_exist<std::vector<std::wstring>>(multiStringGetFn);
        verify_wrong_type<std::vector<std::wstring>, uint32_t>(dwordValueName, test_dword_zero, multiStringGetFn, dwordSetFn);
        // TODO: string c_str()
        verify_wrong_type<std::vector<std::wstring>, PCWSTR>(stringValueName, test_string_empty.c_str(), multiStringGetFn, stringSetFn);

        // TODO: expanded strings
        // TODO: byte vectors
    }

    SECTION("typed get and set: with string key")
    {
        // Lifted for testing wrong type set
        const auto qwordSetFn = [](PCWSTR valueName, uint64_t input) -> void { wil::reg::set_value_qword(HKEY_CURRENT_USER, testSubkey, valueName, input); };

        // DWORDs
        const auto dwordGetFn = [](PCWSTR valueName) -> DWORD { return wil::reg::get_value_dword(HKEY_CURRENT_USER, testSubkey, valueName); };
        const auto dwordSetFn = [](PCWSTR valueName, uint32_t input) -> void { wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, input); };
        for (const auto& value : dwordTestArray)
        {
            verify_good<DWORD, uint32_t>(
                dwordValueName,
                value,
                dwordGetFn,
                dwordSetFn);
        }
        verify_not_exist<DWORD>(dwordGetFn);
        verify_wrong_type<DWORD, uint64_t>(qwordValueName, test_qword_zero, dwordGetFn, qwordSetFn);

        // QWORDs
        const auto qwordGetFn = [](PCWSTR valueName) -> DWORD64 { return wil::reg::get_value_qword(HKEY_CURRENT_USER, testSubkey, valueName); };
        for (const auto& value : qwordTestArray)
        {
            verify_good<DWORD64, uint64_t>(
                qwordValueName,
                value,
                qwordGetFn,
                qwordSetFn);
        }
        verify_not_exist<DWORD64>(qwordGetFn);
        verify_wrong_type<DWORD64, uint32_t>(dwordValueName, test_dword_zero, qwordGetFn, dwordSetFn);

        // Elevated for testing wrong type set
        const auto multistringSetFn = [](PCWSTR valueName, std::vector<std::wstring> const& input) -> void { wil::reg::set_value_multistring(HKEY_CURRENT_USER, testSubkey, valueName, input); };

        // TODO: multiple string types
        const auto stringGetFn = [](PCWSTR valueName) -> std::wstring { return wil::reg::get_value_string(HKEY_CURRENT_USER, testSubkey, valueName); };
        const auto stringSetFn = [](PCWSTR valueName, PCWSTR input) -> void { wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, valueName, input); };
        for (const auto& value : stringTestArray)
        {
            verify_good<std::wstring, PCWSTR>(
                stringValueName,
                value.c_str(),
                stringGetFn,
                stringSetFn);
        }
        verify_not_exist<std::wstring>(stringGetFn);
        verify_wrong_type<std::wstring, uint32_t>(dwordValueName, test_dword_zero, stringGetFn, dwordSetFn);
        verify_wrong_type<std::wstring, std::vector<std::wstring>>(multiStringValueName, test_multistring_empty, stringGetFn, multistringSetFn);

        const auto multiStringGetFn = [](PCWSTR valueName) -> std::vector<std::wstring> { return wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, valueName); };
        for (const auto& value : multiStringTestArray)
        {
            verify_good<std::vector<std::wstring>, std::vector<std::wstring>>(
                multiStringValueName,
                value,
                multiStringGetFn,
                multistringSetFn);
        }
        verify_not_exist<std::vector<std::wstring>>(multiStringGetFn);
        verify_wrong_type<std::vector<std::wstring>, uint32_t>(dwordValueName, test_dword_zero, multiStringGetFn, dwordSetFn);
        // TODO: string c_str()
        verify_wrong_type<std::vector<std::wstring>, PCWSTR>(stringValueName, test_string_empty.c_str(), multiStringGetFn, stringSetFn);

        // TODO: expanded strings
        // TODO: byte vectors
    }
#endif
}

namespace
{
    struct DwordFns
    {
        using RetType = DWORD;
        using SetType = uint32_t;
        
        static std::vector<RetType> testValues() { return dwordTestVector; }
        static PCWSTR testValueName() { return dwordValueName; }

        static std::vector<std::function<HRESULT(wil::unique_hkey const&, PCWSTR)>> set_wrong_value_fns_openkey()
        {
            return {
                [](wil::unique_hkey const& key, PCWSTR value_name) { return wil::reg::set_value_qword_nothrow(key.get(), value_name, test_qword_zero); }
            };
        }

        static std::vector<std::function<HRESULT(HKEY, PCWSTR, PCWSTR)>> set_wrong_value_fns_subkey()
        {
            return {
                [](HKEY key, PCWSTR subkey, PCWSTR value_name) { return wil::reg::set_value_qword_nothrow(key, subkey, value_name, test_qword_zero); }
            };
        }

        static HRESULT set_nothrow(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value) { return wil::reg::set_value_dword_nothrow(key.get(), valueName, value); }
        static HRESULT set_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value) { return wil::reg::set_value_dword_nothrow(key, subkey, valueName, value); }

        static HRESULT get_nothrow(wil::unique_hkey const& key, PCWSTR valueName, RetType* output) { return wil::reg::get_value_dword_nothrow(key.get(), valueName, output); }
        static HRESULT get_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, RetType* output) { return wil::reg::get_value_dword_nothrow(key, subkey, valueName, output); }

#if defined(WIL_ENABLE_EXCEPTIONS)
        static void set(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value) { wil::reg::set_value_dword(key.get(), valueName, value); }
        static void set(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value) { wil::reg::set_value_dword(key, subkey, valueName, value); }

        static RetType get(wil::unique_hkey const& key, PCWSTR valueName) { return wil::reg::get_value_dword(key.get(), valueName); }
        static RetType get(HKEY key, PCWSTR subkey, PCWSTR valueName) { return wil::reg::get_value_dword(key, subkey, valueName); }
#if defined(__cpp_lib_optional)
        static std::optional<RetType> try_get(wil::unique_hkey const& key, PCWSTR valueName) { return wil::reg::try_get_value_dword(key.get(), valueName); }
        static std::optional<RetType> try_get(HKEY key, PCWSTR subkey, PCWSTR valueName) { return wil::reg::try_get_value_dword(key, subkey, valueName); }
#endif // defined(__cpp_lib_optional)
#endif // defined(WIL_ENABLE_EXCEPTIONS)
    };

    struct QwordFns
    {
        using RetType = DWORD64;
        using SetType = uint64_t;

        static std::vector<RetType> testValues() { return qwordTestVector; }
        static PCWSTR testValueName() { return qwordValueName; }

        static std::vector<std::function<HRESULT(wil::unique_hkey const&, PCWSTR)>> set_wrong_value_fns_openkey()
        {
            return {
                [](wil::unique_hkey const& key, PCWSTR value_name) { return wil::reg::set_value_dword_nothrow(key.get(), value_name, test_dword_zero); }
            };
        }

        static std::vector<std::function<HRESULT(HKEY, PCWSTR, PCWSTR)>> set_wrong_value_fns_subkey()
        {
            return {
                [](HKEY key, PCWSTR subkey, PCWSTR value_name) { return wil::reg::set_value_dword_nothrow(key, subkey, value_name, test_dword_zero); }
            };
        }

        static HRESULT set_nothrow(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value) { return wil::reg::set_value_qword_nothrow(key.get(), valueName, value); }
        static HRESULT set_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value) { return wil::reg::set_value_qword_nothrow(key, subkey, valueName, value); }

        static HRESULT get_nothrow(wil::unique_hkey const& key, PCWSTR valueName, RetType* output) { return wil::reg::get_value_qword_nothrow(key.get(), valueName, output); }
        static HRESULT get_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, RetType* output) { return wil::reg::get_value_qword_nothrow(key, subkey, valueName, output); }

#if defined(WIL_ENABLE_EXCEPTIONS)
        static void set(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value) { wil::reg::set_value_qword(key.get(), valueName, value); }
        static void set(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value) { wil::reg::set_value_qword(key, subkey, valueName, value); }

        static RetType get(wil::unique_hkey const& key, PCWSTR valueName) { return wil::reg::get_value_qword(key.get(), valueName); }
        static RetType get(HKEY key, PCWSTR subkey, PCWSTR valueName) { return wil::reg::get_value_qword(key, subkey, valueName); }

#if defined(__cpp_lib_optional)
        static std::optional<RetType> try_get(wil::unique_hkey const& key, PCWSTR valueName) { return wil::reg::try_get_value_qword(key.get(), valueName); }
        static std::optional<RetType> try_get(HKEY key, PCWSTR subkey, PCWSTR valueName) { return wil::reg::try_get_value_qword(key, subkey, valueName); }
#endif // defined(__cpp_lib_optional)
#endif // defined(WIL_ENABLE_EXCEPTIONS)
    };

#if defined(WIL_ENABLE_EXCEPTIONS)
    struct MultiStringFns
    {
        using RetType = std::vector<std::wstring>;
        using SetType = std::vector<std::wstring>;

        static std::vector<RetType> testValues() { return multiStringTestVector; }
        static PCWSTR testValueName() { return multiStringValueName; }

        static std::vector<std::function<HRESULT(wil::unique_hkey const&, PCWSTR)>> set_wrong_value_fns_openkey()
        {
            return {
                [](wil::unique_hkey const& key, PCWSTR value_name) { return wil::reg::set_value_dword_nothrow(key.get(), value_name, test_dword_zero); },
                [](wil::unique_hkey const& key, PCWSTR value_name) { return wil::reg::set_value_string_nothrow(key.get(), value_name, test_string_empty.c_str()); },
            };
        }

        static std::vector<std::function<HRESULT(HKEY, PCWSTR, PCWSTR)>> set_wrong_value_fns_subkey()
        {
            return {
                [](HKEY key, PCWSTR subkey, PCWSTR value_name) { return wil::reg::set_value_dword_nothrow(key, subkey, value_name, test_dword_zero); },
                [](HKEY key, PCWSTR subkey, PCWSTR value_name) { return wil::reg::set_value_string_nothrow(key, subkey, value_name, test_string_empty.c_str()); },
            };
        }

        static HRESULT set_nothrow(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value) { return wil::reg::set_value_multistring_nothrow(key.get(), valueName, value); }
        static HRESULT set_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value) { return wil::reg::set_value_multistring_nothrow(key, subkey, valueName, value); }

        static HRESULT get_nothrow(wil::unique_hkey const& key, PCWSTR valueName, RetType* output) { return wil::reg::get_value_multistring_nothrow(key.get(), valueName, output); }
        static HRESULT get_nothrow(HKEY key, PCWSTR subkey, PCWSTR valueName, RetType* output) { return wil::reg::get_value_multistring_nothrow(key, subkey, valueName, output); }

        static void set(wil::unique_hkey const& key, PCWSTR valueName, SetType const& value) { wil::reg::set_value_multistring(key.get(), valueName, value); }
        static void set(HKEY key, PCWSTR subkey, PCWSTR valueName, SetType const& value) { wil::reg::set_value_multistring(key, subkey, valueName, value); }

        static RetType get(wil::unique_hkey const& key, PCWSTR valueName) { return wil::reg::get_value_multistring(key.get(), valueName); }
        static RetType get(HKEY key, PCWSTR subkey, PCWSTR valueName) { return wil::reg::get_value_multistring(key, subkey, valueName); }

#if defined(__cpp_lib_optional)
        static std::optional<RetType> try_get(wil::unique_hkey const& key, PCWSTR valueName) { return wil::reg::try_get_value_multistring(key.get(), valueName); }
        static std::optional<RetType> try_get(HKEY key, PCWSTR subkey, PCWSTR valueName) { return wil::reg::try_get_value_multistring(key, subkey, valueName); }
#endif // defined(__cpp_lib_optional)
    };
#endif // defined(WIL_ENABLE_EXCEPTIONS)

#if defined(WIL_ENABLE_EXCEPTIONS)
using AllRegistryTypes = std::tuple<DwordFns, QwordFns, MultiStringFns>;
#endif // defined(WIL_ENABLE_EXCEPTIONS)

using ExceptionSafeTypes = std::tuple<DwordFns, QwordFns>;

}

TEMPLATE_LIST_TEST_CASE("BasicRegistryTests::typed nothrow gets/sets", "[registry]", ExceptionSafeTypes)
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
                REQUIRE_SUCCEEDED(TestType::set_nothrow(hkey, TestType::testValueName(), value));
                typename TestType::RetType result{};
                REQUIRE_SUCCEEDED(TestType::get_nothrow(hkey, TestType::testValueName(), &result));
                REQUIRE(result == value);

                // and verify default value name
                REQUIRE_SUCCEEDED(TestType::set_nothrow(hkey, nullptr, value));
                result = {};
                REQUIRE_SUCCEEDED(TestType::get_nothrow(hkey, nullptr, &result));
                REQUIRE(result == value);
            }

            // fail get* if the value doesn't exist
            typename TestType::RetType result{};
            HRESULT hr = TestType::get_nothrow(hkey, invalidValueName, &result);
            REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

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
                REQUIRE_SUCCEEDED(TestType::set_nothrow(HKEY_CURRENT_USER, testSubkey, TestType::testValueName(), value));
                typename TestType::RetType result{};
                REQUIRE_SUCCEEDED(TestType::get_nothrow(HKEY_CURRENT_USER, testSubkey, TestType::testValueName(), &result));
                REQUIRE(result == value);

                // and verify default value name
                REQUIRE_SUCCEEDED(TestType::set_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, value));
                result = {};
                REQUIRE_SUCCEEDED(TestType::get_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
                REQUIRE(result == value);
            }

            // fail get* if the value doesn't exist
            typename TestType::RetType result{};
            HRESULT hr = TestType::get_nothrow(HKEY_CURRENT_USER, testSubkey, invalidValueName, &result);
            REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

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
TEMPLATE_LIST_TEST_CASE("BasicRegistryTests::typed gets/sets/try_gets", "[registry]", AllRegistryTypes)
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
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
                {
                    const auto ignored = TestType::get(hkey, invalidValueName);
                    ignored;
                });

            // fail if get* requests the wrong type
            for (auto& setWrongTypeFn : TestType::set_wrong_value_fns_openkey())
            {
                REQUIRE_SUCCEEDED(setWrongTypeFn(hkey, wrongTypeValueName));
                VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
                    {
                        const auto ignored = TestType::get(hkey, wrongTypeValueName);
                        ignored;
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
            VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
                {
                    const auto ignored = TestType::get(HKEY_CURRENT_USER, testSubkey, invalidValueName);
                    ignored;
                });

            // fail if get* requests the wrong type
            for (auto& setWrongTypeFn : TestType::set_wrong_value_fns_subkey())
            {
                REQUIRE_SUCCEEDED(setWrongTypeFn(HKEY_CURRENT_USER, testSubkey, wrongTypeValueName));
                VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
                    {
                        const auto ignored = TestType::get(HKEY_CURRENT_USER, testSubkey, wrongTypeValueName);
                        ignored;
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
                VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
                    {
                        const auto ignored = TestType::try_get(hkey, wrongTypeValueName);
                        ignored;
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
                VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
                    {
                        const auto ignored = TestType::try_get(HKEY_CURRENT_USER, testSubkey, wrongTypeValueName);
                        ignored;
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
        DWORD expectedSize{};
        auto hr = wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA));
        REQUIRE(expectedSize == 12);
        WCHAR valid_buffer_result[5]{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(hkey.get(), stringValueName, valid_buffer_result, &expectedSize));
        REQUIRE(expectedSize == 10);
        REQUIRE(0 == wcscmp(valid_buffer_result, L"Test"));

        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_string_nothrow(hkey.get(), invalidValueName, too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        REQUIRE(expectedSize == 0);

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
        DWORD expectedSize{};
        auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA));
        REQUIRE(expectedSize == 12); // yes, this is a registry oddity that it returned 2-bytes-more-than-required
        WCHAR valid_buffer_result[5]{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, valid_buffer_result, &expectedSize));
        REQUIRE(expectedSize == 10);
        REQUIRE(0 == wcscmp(valid_buffer_result, L"Test"));

        // fail get* if the value doesn't exist
        hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, invalidValueName, too_small_result, &expectedSize);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
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
    template<typename StringT, typename SetStringT = PCWSTR>
    void verify_string_nothrow(
        std::function<HRESULT(PCWSTR, typename type_identity<StringT>::type&)> getFn,
        std::function<HRESULT(PCWSTR, typename type_identity<SetStringT>::type)> setFn,
        std::function<HRESULT(PCWSTR)> wrongSetFn)
    {
        for (const auto& value : stringTestArray)
        {
            REQUIRE_SUCCEEDED(setFn(stringValueName, value.c_str()));
            StringT result{};
            REQUIRE_SUCCEEDED(getFn(stringValueName, result));
            REQUIRE(AreStringsEqual(result, value));

            // and verify default value name
            REQUIRE_SUCCEEDED(setFn(nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(getFn(nullptr, result));
            REQUIRE(AreStringsEqual(result, value));
        }

        // fail get* if the value doesn't exist
        StringT result{};
        HRESULT hr = getFn(invalidValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wrongSetFn(dwordValueName));
        hr = getFn(dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    template<typename StringT>
    void verify_string_nothrow(HKEY key)
    {
        verify_string_nothrow<StringT>(
            [&key](PCWSTR valueName, StringT& output) { return wil::reg::get_value_string_nothrow(key, valueName, output); },
            [&key](PCWSTR valueName, PCWSTR input) { return wil::reg::set_value_string_nothrow(key, valueName, input); },
            [&key](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_dword_nothrow(key, valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_string_nothrow(HKEY key, PCWSTR subkey)
    {
        verify_string_nothrow<StringT>(
            [&key, &subkey](PCWSTR valueName, StringT& output) { return wil::reg::get_value_string_nothrow(key, subkey, valueName, output); },
            [&key, &subkey](PCWSTR valueName, PCWSTR input) { return wil::reg::set_value_string_nothrow(key, subkey, valueName, input); },
            [&key, &subkey](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_dword_nothrow(key, subkey, valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_string_generic_get_value_nothrow(HKEY key)
    {
        // TODO: this dereferencing loses the wil::unique type
        verify_string_nothrow<StringT>(
            [&key](PCWSTR valueName, StringT& output) { return wil::reg::get_value_nothrow(key, valueName, &output); },
            [&key](PCWSTR valueName, PCWSTR input) { return wil::reg::set_value_nothrow(key, valueName, input); },
            [&key](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_dword_nothrow(key, valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_string_generic_get_value_nothrow(HKEY key, PCWSTR subkey)
    {
        // TODO: this dereferencing loses the wil::unique type
        verify_string_nothrow<StringT>(
            [&key, &subkey](PCWSTR valueName, StringT& output) { return wil::reg::get_value_nothrow(key, subkey, valueName, &output); },
            [&key, &subkey](PCWSTR valueName, PCWSTR input) { return wil::reg::set_value_nothrow(key, subkey, valueName, input); },
            [&key, &subkey](PCWSTR valueName) -> HRESULT { return wil::reg::set_value_dword_nothrow(key, subkey, valueName, test_dword_zero); });
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    template<typename StringT, typename StringSetT = PCWSTR>
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
            {
                const auto ignored = getFn(invalidValueName);
                ignored;
            });

        // fail if get* requests the wrong type
        setWrongTypeFn(dwordValueName);
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
            {
                const auto ignored = getFn(dwordValueName);
                ignored;
            });
    }

    template<typename StringT>
    void verify_string()
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_string<StringT>(
            [&hkey](PCWSTR valueName) { return wil::reg::get_value_string<StringT>(hkey.get(), valueName); },
            [&hkey](PCWSTR valueName, PCWSTR value) -> void { wil::reg::set_value_string(hkey.get(), valueName, value); },
            [&hkey](PCWSTR valueName) { wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_string_subkey()
    {
        verify_string<StringT>(
            [](PCWSTR valueName) { return wil::reg::get_value_string<StringT>(HKEY_CURRENT_USER, testSubkey, valueName); },
            [](PCWSTR valueName, PCWSTR value) -> void { wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, valueName, value); },
            [](PCWSTR valueName) { wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_string_generic_get_value()
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_string<StringT>(
            [&hkey](PCWSTR valueName) { return wil::reg::get_value<StringT>(hkey.get(), valueName); },
            [&hkey](PCWSTR valueName, PCWSTR value) -> void { wil::reg::set_value(hkey.get(), valueName, value); },
            [&hkey](PCWSTR valueName) { wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_string_generic_get_value_subkey()
    {
        verify_string<StringT>(
            [](PCWSTR valueName) { return wil::reg::get_value<StringT>(HKEY_CURRENT_USER, testSubkey, valueName); },
            [](PCWSTR valueName, PCWSTR value) -> void { wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, valueName, value); },
            [](PCWSTR valueName) { wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });
    }

#if defined(__cpp_lib_optional)
    template<typename StringT, typename StringSetT = PCWSTR>
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
            {
                const auto ignored = tryGetFn(dwordValueName);
                ignored;
            });
    }

    template<typename StringT>
    void verify_try_string()
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_try_string<StringT>(
            [&hkey](PCWSTR valueName) { return wil::reg::try_get_value_string<StringT>(hkey.get(), valueName); },
            [&hkey](PCWSTR valueName, PCWSTR value) -> void { wil::reg::set_value_string(hkey.get(), valueName, value); },
            [&hkey](PCWSTR valueName) { wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_try_string_subkey()
    {
        verify_try_string<StringT>(
            [](PCWSTR valueName) { return wil::reg::try_get_value_string<StringT>(HKEY_CURRENT_USER, testSubkey, valueName); },
            [](PCWSTR valueName, PCWSTR value) -> void { wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, valueName, value); },
            [](PCWSTR valueName) { wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_try_string_generic_get_value()
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_try_string<StringT>(
            [&hkey](PCWSTR valueName) { return wil::reg::try_get_value<StringT>(hkey.get(), valueName); },
            [&hkey](PCWSTR valueName, PCWSTR value) -> void { wil::reg::set_value(hkey.get(), valueName, value); },
            [&hkey](PCWSTR valueName) { wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_try_string_generic_get_value_subkey()
    {
        verify_try_string<StringT>(
            [](PCWSTR valueName) { return wil::reg::try_get_value<StringT>(HKEY_CURRENT_USER, testSubkey, valueName); },
            [](PCWSTR valueName, PCWSTR value) -> void { wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, valueName, value); },
            [](PCWSTR valueName) { wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });
    }
#endif // defined(__cpp_lib_optional)
#endif
}

TEST_CASE("BasicRegistryTests::string types", "[registry]")
{
    SECTION("set_value_string_nothrow/get_value_string_nothrow: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

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

#if defined(__WIL_OLEAUTO_H_)
        verify_string_generic_get_value_nothrow<wil::unique_bstr>(hkey.get());
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string_generic_get_value_nothrow<wil::shared_bstr>(hkey.get());
#endif
#endif

        // TODO: crashes?
#if defined(__WIL_OBJBASE_H_)
        //verify_string_generic_get_value_nothrow<wil::unique_cotaskmem_string>(hkey.get());
#if defined(__WIL_OBJBASE_H_STL)
        //verify_string_generic_get_value_nothrow<wil::shared_cotaskmem_string>(hkey.get());
#endif
#endif
    }

    SECTION("strings set_value_nothrow/get_value_nothrow: with string key")
    {
#if defined(__WIL_OLEAUTO_H_)
        verify_string_generic_get_value_nothrow<wil::unique_bstr>(HKEY_CURRENT_USER, testSubkey);
#if defined(__WIL_OLEAUTO_H_STL)
        verify_string_generic_get_value_nothrow<wil::shared_bstr>(HKEY_CURRENT_USER, testSubkey);
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        //verify_string_generic_get_value_nothrow<wil::unique_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey);
#if defined(__WIL_OBJBASE_H_STL)
        //verify_string_generic_get_value_nothrow<wil::shared_cotaskmem_string>(HKEY_CURRENT_USER, testSubkey);
#endif
#endif
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("set_value_string/get_value_string: with opened key")
    {
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

    // TODO: it's a bug that we support unique here, right?
    SECTION("strings set_value/try_get_value: with open key")
    {
        verify_try_string_generic_get_value<std::wstring>();

#if defined(__WIL_OLEAUTO_H_)
        verify_try_string_generic_get_value<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_string_generic_get_value<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_try_string_generic_get_value<wil::unique_cotaskmem_string>();
#if defined(__WIL_OBJBASE_H_STL)
        verify_try_string_generic_get_value<wil::shared_cotaskmem_string>();
#endif
#endif
    }

    SECTION("strings set_value/try_get_value: with string key")
    {
        verify_try_string_generic_get_value_subkey<std::wstring>();

#if defined(__WIL_OLEAUTO_H_)
        verify_try_string_generic_get_value_subkey<wil::unique_bstr>();
#if defined(__WIL_OLEAUTO_H_STL)
        verify_try_string_generic_get_value_subkey<wil::shared_bstr>();
#endif
#endif

#if defined(__WIL_OBJBASE_H_)
        verify_try_string_generic_get_value_subkey<wil::unique_cotaskmem_string>();
#if defined(__WIL_OBJBASE_H_STL)
        verify_try_string_generic_get_value_subkey<wil::shared_cotaskmem_string>();
#endif
#endif
    }
#endif // defined(__cpp_lib_optional)

#endif
}

#if defined(WIL_ENABLE_EXCEPTIONS)
TEST_CASE("BasicRegistryTests::expanded_wstring", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    // TODO: wchar arrays
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
        hr = wil::reg::get_value_expanded_string_nothrow(hkey.get(), invalidValueName, result);
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
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, invalidValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        hr = wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    // TODO: verify string is the default
}
#endif

namespace
{
#if defined(WIL_ENABLE_EXCEPTIONS)
    template<typename StringT, typename SetStringT = PCWSTR>
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

            REQUIRE_SUCCEEDED(setFn(stringValueName, value.c_str()));
            StringT result{};
            REQUIRE_SUCCEEDED(getFn(stringValueName, result));
            REQUIRE(AreStringsEqual(result, expanded_value));

            // and verify default value name
            REQUIRE_SUCCEEDED(setFn(nullptr, value.c_str()));
            result = {};
            REQUIRE_SUCCEEDED(getFn(nullptr, result));
            REQUIRE(AreStringsEqual(result, expanded_value));
        }

        // fail get* if the value doesn't exist
        StringT result{};
        auto hr = getFn(invalidValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(setWrongTypeFn(dwordValueName));
        hr = getFn(dwordValueName, result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }

    template<typename StringT>
    void verify_expanded_string_nothrow()
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_expanded_string_nothrow<StringT>(
            [&hkey](PCWSTR valueName, StringT& output) { return wil::reg::get_value_expanded_string_nothrow(hkey.get(), valueName, output); },
            [&hkey](PCWSTR valueName, PCWSTR input) { return wil::reg::set_value_expanded_string_nothrow(hkey.get(), valueName, input); },
            [&hkey](PCWSTR valueName) { return wil::reg::set_value_dword_nothrow(hkey.get(), valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_expanded_string_subkey_nothrow()
    {
        verify_expanded_string_nothrow<StringT>(
            [](PCWSTR valueName, StringT& output) { return wil::reg::get_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, output); },
            [](PCWSTR valueName, PCWSTR input) { return wil::reg::set_value_expanded_string_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); },
            [](PCWSTR valueName) { return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });
    }

    template<typename StringT, typename SetStringT = PCWSTR>
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
            {
                getFn(invalidValueName);
            });

        // fail if get* requests the wrong type
        setWrongTypeFn(dwordValueName);
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
            {
                getFn(dwordValueName);
            });
    }

    template<typename StringT>
    void verify_expanded_string()
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_expanded_string<StringT>(
            [&hkey](PCWSTR valueName) -> StringT { return wil::reg::get_value_expanded_string<StringT>(hkey.get(), valueName); },
            [&hkey](PCWSTR valueName, PCWSTR input) { wil::reg::set_value_expanded_string(hkey.get(), valueName, input); },
            [&hkey](PCWSTR valueName) { wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_expanded_string_subkey()
    {
        verify_expanded_string<StringT>(
            [](PCWSTR valueName) -> StringT { return wil::reg::get_value_expanded_string<StringT>(HKEY_CURRENT_USER, testSubkey, valueName); },
            [](PCWSTR valueName, PCWSTR input) { wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, valueName, input); },
            [](PCWSTR valueName) { wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });
    }

#if defined(__cpp_lib_optional)
    template<typename StringT, typename SetStringT = PCWSTR>
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
            {
                getFn(dwordValueName);
            });
    }

    template<typename StringT>
    void verify_try_expanded_string()
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_try_expanded_string<StringT>(
            [&hkey](PCWSTR valueName) { return wil::reg::try_get_value_expanded_string<StringT>(hkey.get(), valueName); },
            [&hkey](PCWSTR valueName, PCWSTR input) { wil::reg::set_value_expanded_string(hkey.get(), valueName, input); },
            [&hkey](PCWSTR valueName) { wil::reg::set_value_dword(hkey.get(), valueName, test_dword_zero); });
    }

    template<typename StringT>
    void verify_try_expanded_string_subkey()
    {
        verify_try_expanded_string<StringT>(
            [](PCWSTR valueName) { return wil::reg::try_get_value_expanded_string<StringT>(HKEY_CURRENT_USER, testSubkey, valueName); },
            [](PCWSTR valueName, PCWSTR input) { wil::reg::set_value_expanded_string(HKEY_CURRENT_USER, testSubkey, valueName, input); },
            [](PCWSTR valueName) { wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero); });
    }

    // TODO: test generic try_get_value
#endif // defined(__cpp_lib_optional)
#endif
}

#if defined(WIL_ENABLE_EXCEPTIONS)
TEST_CASE("BasicRegistryTests::expanded_string", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_expanded_string_nothrow/get_value_expanded_string_nothrow: with opened key")
    {
        verify_expanded_string_nothrow<std::wstring>();

// TODO: redundant?
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
        verify_expanded_string_subkey_nothrow<std::wstring>();

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

    SECTION("set_value_expanded_string/get_value_expanded_string: with opened key")
    {
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

    // TODO: verify string is the default
}
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

#ifdef __WIL_WINREG_STL
TEST_CASE("BasicRegistryTests::multi-strings", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("set_value_multistring_nothrow/get_value_multistring_nothrow: empty array with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
        // thus the result should have one empty string
        std::vector<std::wstring> arrayOfOne{ L"" };
        REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(hkey.get(), stringValueName, test_multistring_empty));
        std::vector<std::wstring> result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(hkey.get(), stringValueName, &result));
        REQUIRE(result == arrayOfOne);

        // and verify default value name
        REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(hkey.get(), nullptr, test_multistring_empty));
        result = {};
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(hkey.get(), nullptr, &result));
        REQUIRE(result == arrayOfOne);
    }
    SECTION("set_value_multistring_nothrow/get_value_multistring_nothrow: empty array with string key")
    {
        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
        // thus the result should have one empty string
        std::vector<std::wstring> arrayOfOne{ L"" };
        REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, test_multistring_empty));
        std::vector<std::wstring> result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result == arrayOfOne);

        // and verify default value name
        REQUIRE_SUCCEEDED(wil::reg::set_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, test_multistring_empty));
        result = {};
        REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
        REQUIRE(result == arrayOfOne);
    }

    SECTION("set_value_nothrow/get_value_nothrow: empty array with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
        // thus the result should have one empty string
        std::vector<std::wstring> arrayOfOne{ L"" };
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), stringValueName, test_multistring_empty));
        std::vector<std::wstring> result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), stringValueName, &result));
        REQUIRE(result == arrayOfOne);

        // and verify default value name
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), nullptr, test_multistring_empty));
        result = {};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(hkey.get(), nullptr, &result));
        REQUIRE(result == arrayOfOne);
    }
    SECTION("set_value_nothrow/get_value_nothrow: empty array with string key")
    {
        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
        // thus the result should have one empty string
        std::vector<std::wstring> arrayOfOne{ L"" };
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, test_multistring_empty));
        std::vector<std::wstring> result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, &result));
        REQUIRE(result == arrayOfOne);

        // and verify default value name
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, test_multistring_empty));
        result = {};
        REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(HKEY_CURRENT_USER, testSubkey, nullptr, &result));
        REQUIRE(result == arrayOfOne);
    }

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("set_value_multistring/get_value_multistring: empty array with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
        // thus the result should have one empty string
        std::vector<std::wstring> arrayOfOne{ L"" };
        wil::reg::set_value_multistring(hkey.get(), stringValueName, test_multistring_empty);
        auto result = wil::reg::get_value_multistring(hkey.get(), stringValueName);
        REQUIRE(result == arrayOfOne);

        // and verify default value name
        wil::reg::set_value_multistring(hkey.get(), nullptr, test_multistring_empty);
        result = wil::reg::get_value_multistring(hkey.get(), nullptr);
        REQUIRE(result == arrayOfOne);
    }
    SECTION("set_value_multistring/get_value_multistring: empty array with string key")
    {
        // When passed an empty array, we write in 2 null-terminators as part of set_value_multistring_nothrow (i.e. a single empty string)
        // thus the result should have one empty string
        std::vector<std::wstring> arrayOfOne{ L"" };
        wil::reg::set_value_multistring(HKEY_CURRENT_USER, testSubkey, stringValueName, test_multistring_empty);
        auto result = wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == arrayOfOne);

        // and verify default value name
        wil::reg::set_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr, test_multistring_empty);
        result = wil::reg::get_value_multistring(HKEY_CURRENT_USER, testSubkey, nullptr);
        REQUIRE(result == arrayOfOne);
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
        const auto result = wil::reg::try_get_value_multistring(hkey.get(), invalidValueName);
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
            {
                wil::reg::try_get_value_multistring(hkey.get(), dwordValueName);
            });
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
        const auto result = wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, invalidValueName);
        REQUIRE(!result);
        REQUIRE(!result.has_value());

        // fail if get* requests the wrong type
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_zero));
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
            {
                wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, testSubkey, dwordValueName);
            });
    }
#endif
#endif
}
#endif

#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
namespace
{
    void verify_byte_vector_nothrow(
        std::function<HRESULT(PCWSTR, DWORD, std::vector<BYTE>*)> getFn,
        std::function<HRESULT(PCWSTR, DWORD, const std::vector<BYTE>&)> setFn,
        std::function<HRESULT(PCWSTR, uint32_t)> setDwordFn)
    {
        for (const auto& value : vectorBytesTestArray)
        {
            REQUIRE_SUCCEEDED(setFn(stringValueName, REG_BINARY, value));
            std::vector<BYTE> result{};
            REQUIRE_SUCCEEDED(getFn(stringValueName, REG_BINARY, &result));
            REQUIRE(result == value);

            // and verify default value name
            REQUIRE_SUCCEEDED(setFn(nullptr, REG_BINARY, value));
            result = {};
            REQUIRE_SUCCEEDED(getFn(nullptr, REG_BINARY, &result));
            REQUIRE(result == value);
        }

        // fail get* if the value doesn't exist
        std::vector<BYTE> result{};
        auto hr = getFn(invalidValueName, REG_BINARY, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // fail if get* requests the wrong type
        hr = getFn(stringValueName, REG_SZ, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
        hr = getFn(stringValueName, REG_DWORD, &result);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));

        // should succeed if we specify the correct type
        REQUIRE_SUCCEEDED(setDwordFn(dwordValueName, 0xffffffff));
        REQUIRE_SUCCEEDED(getFn(dwordValueName, REG_DWORD, &result));
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), [&]()
            {
                getFn(invalidValueName, REG_BINARY);
            });

        // fail if get* requests the wrong type
        setDwordFn(dwordValueName, 0xffffffff);
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
            {
                getFn(dwordValueName, REG_BINARY);
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
        VerifyThrowsHr(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), [&]()
            {
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
#endif
}

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

        verify_byte_vector_nothrow(
            [&hkey](PCWSTR valueName, DWORD type, std::vector<BYTE>* output) { return wil::reg::get_value_byte_vector_nothrow(hkey.get(), valueName, type, output); },
            [&hkey](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) { return wil::reg::set_value_byte_vector_nothrow(hkey.get(), valueName, type, input); },
            [&hkey](PCWSTR valueName, DWORD input) { return wil::reg::set_value_dword_nothrow(hkey.get(), valueName, input); });
    }
    SECTION("set_value_byte_vector_nothrow/get_value_byte_vector_nothrow: with string key")
    {
        verify_byte_vector_nothrow(
            [](PCWSTR valueName, DWORD type, std::vector<BYTE>* output) { return wil::reg::get_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, type, output); },
            [](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) { return wil::reg::set_value_byte_vector_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, type, input); },
            [](PCWSTR valueName, DWORD input) { return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, valueName, input); });
    }
    SECTION("set_value_byte_vector/get_value_byte_vector: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_byte_vector(
            [&hkey](PCWSTR valueName, DWORD type) { return wil::reg::get_value_byte_vector(hkey.get(), valueName, type); },
            [&hkey](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) { wil::reg::set_value_byte_vector(hkey.get(), valueName, type, input); },
            [&hkey](PCWSTR valueName, DWORD input) { wil::reg::set_value_dword(hkey.get(), valueName, input); });
    }
    SECTION("set_value_byte_vector/get_value_byte_vector: with string key")
    {
        verify_byte_vector(
            [](PCWSTR valueName, DWORD type) { return wil::reg::get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, valueName, type); },
            [](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) { wil::reg::set_value_byte_vector(HKEY_CURRENT_USER, testSubkey, valueName, type, input); },
            [](PCWSTR valueName, DWORD input) { wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, input); });
    }

#if defined(__cpp_lib_optional)
    SECTION("set_value_byte_vector/try_get_value_byte_vector: with open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        verify_try_byte_vector(
            [&hkey](PCWSTR valueName, DWORD type) { return wil::reg::try_get_value_byte_vector(hkey.get(), valueName, type); },
            [&hkey](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) { wil::reg::set_value_byte_vector(hkey.get(), valueName, type, input); },
            [&hkey](PCWSTR valueName, DWORD input) { wil::reg::set_value_dword(hkey.get(), valueName, input); });
    }

    SECTION("set_value/try_get_value_byte_vector: with string key")
    {
        verify_try_byte_vector(
            [](PCWSTR valueName, DWORD type) { return wil::reg::try_get_value_byte_vector(HKEY_CURRENT_USER, testSubkey, valueName, type); },
            [](PCWSTR valueName, DWORD type, const std::vector<BYTE>& input) { wil::reg::set_value_byte_vector(HKEY_CURRENT_USER, testSubkey, valueName, type, input); },
            [](PCWSTR valueName, DWORD input) { wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, input); });
    }
#endif
}
#endif