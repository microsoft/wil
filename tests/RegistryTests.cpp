#include "pch.h"

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

namespace
{
// RegSetValueExW stores an extra WCHAR for even-sized, unterminated string data when the WCHAR past cbData is NUL.
// Write from a copy padded with a nonzero WCHAR so the stored extent is exactly the requested bytes.
LSTATUS set_value_exact_bytes(HKEY key, PCWSTR name, DWORD type, const std::vector<BYTE>& data, DWORD bytes)
{
    std::vector<BYTE> padded(data.begin(), data.begin() + bytes);
    padded.insert(padded.end(), {0xFF, 0xFF});
    return ::RegSetValueExW(key, name, 0, type, padded.data(), bytes);
}

struct appended_multi_layout
{
    const char* id;
    size_t nullWidth;
    std::vector<BYTE> bytes;
    DWORD queryBytes;
    std::vector<BYTE> nativeBytes;
    std::vector<std::wstring> expected;
};

std::vector<appended_multi_layout> appended_multi_layouts()
{
    const std::vector<BYTE> prefix{'a', 0, 'b', 0, 'c', 0, 0, 0, 'd', 0, 'e', 0, 'f', 0, 0, 0};
    const auto append = [&](std::initializer_list<BYTE> tail) {
        auto bytes = prefix;
        bytes.insert(bytes.end(), tail);
        return bytes;
    };
    // These fixtures model native query/read extents separately from the current walker's terminal-marker policy.
    // A lone byte cannot be skipped to realign: N1 completes A, not a WCHAR separator (C3N1 yields Aghi); N2 shifts code units.
    // C1's found terminator is the last marker, so A is omitted; missing-terminator fixtures gain meaningful native nulls.
    return {
        {"P", 0, append({0x41}), 21, append({0, 0}), {L"abc", L"def"}},
        {"old-double", 0, append({0, 0, 0x41}), 23, append({0, 0, 0x41}), {L"abc", L"def"}},
        {"C1N1", 1, append({0x41, 0}), 22, append({0x41, 0, 0, 0}), {L"abc", L"def"}},
        {"C1N2", 2, append({0x41, 0, 0}), 23, append({0x41, 0, 0, 0}), {L"abc", L"def"}},
        {"C3N1", 1, append({0x41, 0, 'g', 0, 'h', 0, 'i', 0, 0, 0, 0, 0}), 32,
            append({0x41, 0, 'g', 0, 'h', 0, 'i', 0, 0, 0, 0, 0}), {L"abc", L"def", L"Aghi"}},
        {"C3N2", 2, append({0x41, 0, 0, 'g', 0, 'h', 0, 'i', 0, 0, 0, 0, 0}), 33,
            append({0x41, 0, 0, 'g', 0, 'h', 0, 'i', 0, 0, 0, 0, 0}), {L"abc", L"def", L"\x0041\x6700\x6800\x6900"}}};
}

void verify_appended_multi_bytes(const BYTE* bytes, size_t count, const appended_multi_layout& layout)
{
    REQUIRE(bytes != nullptr);
    REQUIRE(count == layout.nativeBytes.size());
    REQUIRE(memcmp(bytes, layout.nativeBytes.data(), count) == 0);
}

void verify_appended_multi_string(PCWSTR data, size_t length, const std::wstring& expected)
{
    REQUIRE(data != nullptr);
    REQUIRE(length == expected.size());
    REQUIRE(data[length] == L'\0');
    REQUIRE(::wcsnlen(data, length + 1) == length);
    REQUIRE(wmemcmp(data, expected.data(), length) == 0);
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::three_parser_multi_appended_null
// Coverage: REG_MULTI_SZ appended-byte/null layouts via native RegGetValueW and whole-WCHAR walking;
// CoTaskMem named/generic nothrow parsers and raw named/reg_view readers are OBJBASE-gated.
// STL plus exceptions enables vector get/try, generic get/try and raw reg_view reads;
// compare native byte extents, element contents and owned terminators without byte realignment.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::three_parser_multi_appended_null", "[registry][registry-three-parser]")
{
    for (const auto& layout : appended_multi_layouts())
    {
        for (size_t route = 0; route < 11; ++route)
        {
#if !WIL_USE_STL || !defined(WIL_ENABLE_EXCEPTIONS)
            if (route >= 6)
            {
                continue;
            }
#endif
#if !defined(__WIL_OBJBASE_H_)
            if (route >= 1 && route <= 4)
            {
                continue;
            }
#endif
            constexpr const char* routes[]{"native", "cotaskmem-named", "cotaskmem-generic", "raw-named", "raw-view",
                                          "whole-wchar-parser", "get", "try", "generic-get", "generic-try", "vector-raw-view"};
            DYNAMIC_SECTION(layout.id << " / N=" << layout.nullWidth << " / " << routes[route])
            {
                wil::unique_hkey key;
                REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(
                    HKEY_CURRENT_USER, testSubkey, key, wil::reg::key_access::readwrite));
                constexpr PCWSTR name = L"AppendedMultiNull";
                auto deleteValue = wil::scope_exit([&] { ::RegDeleteValueW(key.get(), name); });
                REQUIRE(ERROR_SUCCESS == set_value_exact_bytes(
                    key.get(), name, REG_MULTI_SZ, layout.bytes, static_cast<DWORD>(layout.bytes.size())));
                BYTE stored[64]{};
                DWORD storedSize = sizeof(stored), storedType = REG_NONE;
                REQUIRE(ERROR_SUCCESS == ::RegQueryValueExW(key.get(), name, nullptr, &storedType, stored, &storedSize));
                REQUIRE(storedType == REG_MULTI_SZ);
                REQUIRE(storedSize == layout.bytes.size());
                REQUIRE(memcmp(stored, layout.bytes.data(), storedSize) == 0);
                if (route == 0 || route == 5)
                {
                    DWORD size = 0, type = REG_NONE;
                    REQUIRE(ERROR_SUCCESS == ::RegGetValueW(key.get(), nullptr, name, RRF_RT_REG_MULTI_SZ, &type, nullptr, &size));
                    REQUIRE(type == REG_MULTI_SZ);
                    REQUIRE(size == layout.queryBytes);
                    wchar_t native[32];
                    memset(native, 0xcc, sizeof(native));
                    size = sizeof(native);
                    REQUIRE(ERROR_SUCCESS == ::RegGetValueW(
                        key.get(), nullptr, name, RRF_RT_REG_MULTI_SZ, &type, native, &size));
                    REQUIRE(type == REG_MULTI_SZ);
                    REQUIRE(size <= sizeof(native));
                    verify_appended_multi_bytes(reinterpret_cast<const BYTE*>(native), size, layout);
                    if (route == 5)
                    {
                        size_t count = 0;
                        wil::reg::reg_view_details::walk_multistring(native + 0, native + size / sizeof(wchar_t),
                            [&](const wchar_t* first, const wchar_t* last) -> bool {
                                REQUIRE(count < layout.expected.size());
                                REQUIRE(first >= native);
                                REQUIRE(last >= first);
                                REQUIRE(last < native + size / sizeof(wchar_t));
                                verify_appended_multi_string(first, static_cast<size_t>(last - first), layout.expected[count++]);
                                return true;
                            });
                        REQUIRE(count == layout.expected.size());
                    }
                }
#if defined(__WIL_OBJBASE_H_)
                else if (route == 1 || route == 2)
                {
                    wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> value;
                    REQUIRE_SUCCEEDED(route == 1 ? wil::reg::get_value_multistring_nothrow(key.get(), name, value) :
                                                  wil::reg::get_value_nothrow(key.get(), name, value));
                    REQUIRE(value.get() != nullptr);
                    REQUIRE(value.size() == layout.expected.size());
                    for (size_t i = 0; i < value.size(); ++i)
                    {
                        const auto length = layout.expected[i].size();
                        verify_appended_multi_string(value[i], length, layout.expected[i]);
                        REQUIRE(wcslen(value[i]) == length);
                    }
                }
                else if (route == 3 || route == 4)
                {
                    wil::unique_cotaskmem_array_ptr<BYTE> value;
                    REQUIRE_SUCCEEDED(route == 3 ? wil::reg::get_value_binary_nothrow(key.get(), name, REG_MULTI_SZ, value) :
                        wil::reg::reg_view_details::reg_view_nothrow{key.get()}.get_value(nullptr, name, value, REG_MULTI_SZ));
                    verify_appended_multi_bytes(value.get(), value.size(), layout);
                }
#endif
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
                else if (route == 10)
                {
                    std::vector<BYTE> value;
                    REQUIRE_SUCCEEDED(wil::reg::reg_view_details::reg_view_nothrow{key.get()}.get_value(
                        nullptr, name, value, REG_MULTI_SZ));
                    verify_appended_multi_bytes(value.data(), value.size(), layout);
                }
                else
                {
                    std::vector<std::wstring> value;
                    if (route == 6)
                    {
                        value = wil::reg::get_value_multistring(key.get(), name);
                    }
                    else if (route == 8)
                    {
                        value = wil::reg::get_value<std::vector<std::wstring>>(key.get(), name);
                    }
                    else
                    {
                        auto optional = route == 7 ? wil::reg::try_get_value_multistring(key.get(), name) :
                            wil::reg::try_get_value<std::vector<std::wstring>>(key.get(), name);
                        REQUIRE(optional.has_value());
                        value = std::move(*optional);
                    }
                    REQUIRE(value.data() != nullptr);
                    REQUIRE(value.size() == layout.expected.size());
                    for (size_t i = 0; i < value.size(); ++i)
                    {
                        verify_appended_multi_string(value[i].c_str(), value[i].size(), layout.expected[i]);
                    }
                }
#endif
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::walk_multistring_callback
// Coverage: walk_multistring passes exact REG_MULTI_SZ element bounds to a stateful, move-only callback;
// false stops at the first or middle element without later callbacks, while all-true traversal visits every element.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::walk_multistring_callback", "[registry]")
{
    // Returning false stops the entire walk, not just the current callback.
    constexpr wchar_t strings[] = L"one\0two\0three\0";
    const wchar_t* const starts[] = {strings, strings + 4, strings + 8};
    constexpr size_t lengths[] = {3, 3, 5};
    for (size_t stopAfter : {1u, 2u, 4u})
    {
        DYNAMIC_SECTION("stop after " << stopAfter)
        {
            size_t calls = 0;
            auto callback = [ownedCalls = std::make_unique<size_t>(0), &calls, &starts, &lengths, stopAfter](
                                const wchar_t* first, const wchar_t* last) mutable -> bool {
                REQUIRE(calls < std::size(starts));
                REQUIRE(*ownedCalls == calls);
                REQUIRE(first == starts[calls]);
                REQUIRE(last == starts[calls] + lengths[calls]);
                ++*ownedCalls;
                ++calls;
                return calls != stopAfter;
            };
            wil::reg::reg_view_details::walk_multistring(std::begin(strings), std::end(strings), std::move(callback));
            REQUIRE(calls == (stopAfter < std::size(starts) ? stopAfter : std::size(starts)));
        }
    }
}

namespace
{
// Supply a bounded, stable value without modifying a registry key.
void registry_allocation_value(
    witest::detoured_thread_function<&::RegGetValueW>& detour, const wchar_t* value, DWORD bytes)
{
    REQUIRE_SUCCEEDED(detour.reset([=](HKEY, PCWSTR, PCWSTR, DWORD, DWORD*, void* data, DWORD* size) -> LSTATUS {
        const auto capacity = *size;
        *size = bytes;
        if (!data)
        {
            return ERROR_SUCCESS;
        }
        if (capacity < bytes)
        {
            return ERROR_MORE_DATA;
        }
        memcpy(data, value, bytes);
        return ERROR_SUCCESS;
    }));
}

#if defined(__WIL_OBJBASE_H_)
void registry_multistring_allocation_failure(size_t failAt, bool empty = false)
{
    // OOM propagates with empty output, no partial success, and cleanup of every preceding allocation, even for empty input.
    constexpr wchar_t strings[] = L"one\0two\0three\0";
    constexpr wchar_t emptyStrings[] = L"\0";
    witest::detoured_thread_function<&::RegGetValueW> read;
    registry_allocation_value(read, empty ? emptyStrings : strings, empty ? sizeof(emptyStrings) : sizeof(strings));
    size_t calls = 0;
    size_t frees = 0;
    std::array<void*, 8> allocations{};
    bool active = false;
    witest::detoured_thread_function<&::CoTaskMemAlloc> allocate;
    witest::detoured_thread_function<&::CoTaskMemFree> release;
    REQUIRE_SUCCEEDED(allocate.reset([&](size_t bytes) -> void* {
        if (!active)
        {
            return ::CoTaskMemAlloc(bytes);
        }
        ++calls;
        if (calls == failAt)
        {
            return nullptr;
        }
        auto value = ::CoTaskMemAlloc(bytes);
        if (calls <= allocations.size())
        {
            allocations[calls - 1] = value;
        }
        return value;
    }));
    REQUIRE_SUCCEEDED(release.reset([&](void* value) {
        if (value)
        {
            for (auto& allocation : allocations)
            {
                if (allocation == value)
                {
                    allocation = nullptr;
                    ++frees;
                    break;
                }
            }
        }
        ::CoTaskMemFree(value);
    }));
    wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> result;
    active = true;
    const auto hr = wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, L"allocation-test", result);
    active = false;
    const bool outputEmpty = result.empty() && !result;
    result.reset();
    REQUIRE(hr == E_OUTOFMEMORY);
    REQUIRE(outputEmpty);
    REQUIRE(calls == failAt);
    REQUIRE(frees == failAt - 1);
}
#endif
} // namespace

#if defined(__WIL_OBJBASE_H_)
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_multistring_array_oom
// Coverage: OBJBASE-gated get_value_multistring_nothrow rejects REG_MULTI_SZ pointer-array allocation OOM;
// leaves empty output, stops allocating and frees all preceding storage.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_multistring_array_oom", "[registry][registry-allocation]")
{
    registry_multistring_allocation_failure(2);
}
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_multistring_first_oom
// Coverage: OBJBASE-gated get_value_multistring_nothrow rejects first REG_MULTI_SZ string allocation OOM;
// leaves empty output, stops allocating and frees all preceding storage.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_multistring_first_oom", "[registry][registry-allocation]")
{
    registry_multistring_allocation_failure(3);
}
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_multistring_middle_oom
// Coverage: OBJBASE-gated get_value_multistring_nothrow rejects middle REG_MULTI_SZ string allocation OOM;
// leaves empty output, stops allocating and frees the array, raw buffer and earlier string.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_multistring_middle_oom", "[registry][registry-allocation]")
{
    registry_multistring_allocation_failure(4);
}
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_multistring_last_oom
// Coverage: OBJBASE-gated get_value_multistring_nothrow rejects last REG_MULTI_SZ string allocation OOM;
// leaves empty output, stops allocating and frees the array, raw buffer and earlier strings.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_multistring_last_oom", "[registry][registry-allocation]")
{
    registry_multistring_allocation_failure(5);
}
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_multistring_empty_oom
// Coverage: OBJBASE-gated get_value_multistring_nothrow rejects array or empty-string allocation OOM for empty MULTI;
// leaves empty output, stops allocating and frees all preceding storage.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_multistring_empty_oom", "[registry][registry-allocation]")
{
    for (size_t failure : {2u, 3u})
    {
        DYNAMIC_SECTION("allocation " << failure)
        {
            registry_multistring_allocation_failure(failure, true);
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_multistring_empty_success
// Coverage: OBJBASE-gated CoTaskMem MULTI conversion returns one allocated empty string for a short null extent;
// a longer extent with no complete strings succeeds with an empty, null array.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_multistring_empty_success", "[registry][registry-allocation]")
{
    SECTION("short extent produces one empty string")
    {
        constexpr wchar_t data[] = L"\0";
        wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> result;
        REQUIRE_SUCCEEDED(wil::reg::reg_view_details::get_cotaskmemstring_array_from_multistring_nothrow(
            std::begin(data), std::end(data), result));
        REQUIRE(result);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0]);
        REQUIRE(result[0][0] == L'\0');
    }
    SECTION("no complete strings is a successful empty array")
    {
        constexpr wchar_t data[] = {L'a', L'b', L'c'};
        wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> result;
        REQUIRE_SUCCEEDED(wil::reg::reg_view_details::get_cotaskmemstring_array_from_multistring_nothrow(
            std::begin(data), std::end(data), result));
        REQUIRE(result.empty());
        REQUIRE_FALSE(result);
    }
}
#endif

#if defined(__WIL_OLEAUTO_H_)
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_raw_bstr_growth
// Coverage: OLEAUTO-gated get_value_string_nothrow grows a preexisting BSTR through its raw BSTR slot;
// successful replacement frees the old allocation once, while payload OOM preserves its pointer until cleanup.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_raw_bstr_growth", "[registry][registry-allocation]")
{
    for (bool fail : {false, true})
    {
        DYNAMIC_SECTION("replacement failure " << fail)
        {
            wil::unique_bstr result(::SysAllocStringLen(L"x", 1));
            REQUIRE(result);
            const auto original = result.get();
            size_t oldFrees = 0;
            size_t newFrees = 0;
            BSTR replacement = nullptr;
            bool active = false;
            witest::detoured_thread_function<&::SysFreeString> release;
            REQUIRE_SUCCEEDED(release.reset([&](BSTR value) {
                oldFrees += value == original;
                newFrees += value && value == replacement;
                ::SysFreeString(value);
            }));
            // Recover the baseline's leaked original even when a REQUIRE fails.
            auto cleanup = wil::scope_exit([&] {
                result.reset();
                if (!oldFrees)
                {
                    ::SysFreeString(original);
                }
            });
            witest::detoured_thread_function<&::SysAllocStringLen> allocate;
            REQUIRE_SUCCEEDED(allocate.reset([&](const OLECHAR* value, UINT length) -> BSTR {
                if (active && fail)
                {
                    return nullptr;
                }
                auto allocated = ::SysAllocStringLen(value, length);
                if (active)
                {
                    replacement = allocated;
                }
                return allocated;
            }));
            witest::detoured_thread_function<&::RegGetValueW> read;
            constexpr wchar_t value[] = L"longer value";
            registry_allocation_value(read, value, sizeof(value));
            active = true;
            const auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, L"allocation-test", result);
            active = false;
            if (fail)
            {
                REQUIRE(hr == E_OUTOFMEMORY);
                REQUIRE(result.get() == original);
                REQUIRE(oldFrees == 0);
            }
            else
            {
                REQUIRE(hr == S_OK);
                REQUIRE(result);
                REQUIRE(::SysStringLen(result.get()) == ARRAYSIZE(value) - 1);
                REQUIRE(wmemcmp(result.get(), value, ARRAYSIZE(value)) == 0);
                REQUIRE(oldFrees == 1);
                REQUIRE(newFrees == 0);
            }
            result.reset();
            REQUIRE(oldFrees == 1);
            REQUIRE(newFrees == (fail ? 0u : 1u));
        }
    }
}
#endif

#if defined(WIL_ENABLE_EXCEPTIONS) && defined(__WIL_OLEAUTO_H_STL) && defined(__WIL_OBJBASE_H_STL)
namespace
{
template <typename Shared, typename Unique>
void registry_shared_allocation_failure()
{
    Unique original;
    REQUIRE_SUCCEEDED(wil::reg::reg_view_details::reg_value_type_info::resize_buffer_bytes(original, 8));
    Shared result(std::move(original));
    auto alias = result;
    const auto old = result.get();
    bool armed = false;
    bool active = false;
    size_t failures = 0;
    size_t payloadFrees = 0;
    void* payload = nullptr;
    witest::detoured_thread_function<static_cast<void* (*)(size_t)>(&::operator new)> controlBlock;
    REQUIRE_SUCCEEDED(controlBlock.reset([&](size_t bytes) -> void* {
        if (armed)
        {
            armed = false;
            ++failures;
            throw std::bad_alloc();
        }
        return ::operator new(bytes);
    }));
    witest::detoured_thread_function<&::SysAllocStringLen> bstrAllocate;
    witest::detoured_thread_function<&::CoTaskMemAlloc> taskAllocate;
    witest::detoured_thread_function<&::SysFreeString> bstrFree;
    witest::detoured_thread_function<&::CoTaskMemFree> taskFree;
    REQUIRE_SUCCEEDED(bstrAllocate.reset([&](const OLECHAR* value, UINT length) -> BSTR {
        auto allocated = ::SysAllocStringLen(value, length);
        if (active)
        {
            payload = allocated;
            armed = allocated != nullptr;
        }
        return allocated;
    }));
    REQUIRE_SUCCEEDED(taskAllocate.reset([&](size_t bytes) -> void* {
        auto allocated = ::CoTaskMemAlloc(bytes);
        if (active)
        {
            payload = allocated;
            armed = allocated != nullptr;
        }
        return allocated;
    }));
    REQUIRE_SUCCEEDED(bstrFree.reset([&](BSTR value) {
        payloadFrees += value && value == payload;
        ::SysFreeString(value);
    }));
    REQUIRE_SUCCEEDED(taskFree.reset([&](void* value) {
        payloadFrees += value && value == payload;
        ::CoTaskMemFree(value);
    }));
    active = true;
    const auto hr = wil::reg::reg_view_details::reg_value_type_info::resize_buffer_bytes(result, 32);
    active = false;
    REQUIRE(hr == E_OUTOFMEMORY);
    REQUIRE(failures == 1);
    REQUIRE(payload != nullptr);
    REQUIRE(payloadFrees == 1);
    REQUIRE(result.get() == old);
    REQUIRE(alias.get() == old);
    REQUIRE(result.use_count() == 2);
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_shared_bstr_resize_oom
// Coverage: Exception-enabled only, with OLEAUTO/OBJBASE shared adapters: direct shared BSTR resize catches
// control-block bad_alloc as OOM, frees the replacement and preserves the original owner and alias.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_shared_bstr_resize_oom", "[registry][registry-allocation]")
{
    registry_shared_allocation_failure<wil::shared_bstr, wil::unique_bstr>();
}
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_shared_cotaskmem_resize_oom
// Coverage: Exception-enabled only, with OLEAUTO/OBJBASE shared adapters: direct shared CoTaskMem resize catches
// control-block bad_alloc as OOM, frees the replacement and preserves the original owner and alias.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_shared_cotaskmem_resize_oom", "[registry][registry-allocation]")
{
    registry_shared_allocation_failure<wil::shared_cotaskmem_string, wil::unique_cotaskmem_string>();
}
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::allocation_shared_bstr_named_oom
// Coverage: Exception-enabled only, with OLEAUTO/OBJBASE shared adapters: named get_value_string_nothrow catches
// initial shared BSTR control-block allocation failure as OOM and leaves the empty output null.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::allocation_shared_bstr_named_oom", "[registry][registry-allocation]")
{
    wil::shared_bstr result;
    bool armed = false;
    size_t failures = 0;
    witest::detoured_thread_function<static_cast<void* (*)(size_t)>(&::operator new)> allocate;
    REQUIRE_SUCCEEDED(allocate.reset([&](size_t bytes) -> void* {
        if (armed)
        {
            armed = false;
            ++failures;
            throw std::bad_alloc();
        }
        return ::operator new(bytes);
    }));
    armed = true;
    const auto hr = wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, L"allocation-test", result);
    REQUIRE(hr == E_OUTOFMEMORY);
    REQUIRE(failures == 1);
    REQUIRE_FALSE(result);
}
#endif

namespace
{
constexpr DWORD threeParserTypes[]{REG_SZ, REG_EXPAND_SZ, REG_MULTI_SZ};
constexpr const char* threeParserNames[]{"REG_SZ", "REG_EXPAND_SZ", "REG_MULTI_SZ"};
constexpr PCWSTR threeParserValue = L"ThreeParserValue";
constexpr PCWSTR threeParserEnvironment = L"WIL_THREE_PARSER_TEST";

struct registry_string_payload
{
    // Raw byte extent is independent of stored WCHAR terminators; expected holds scalar or MULTI element semantics.
    std::vector<BYTE> bytes;
    std::vector<std::wstring> expected;
    std::wstring ownedScalar;
};

struct registry_string_scenario
{
    const char* name;
    std::array<registry_string_payload, 3> payload;
    bool growValue = false;
    bool growEnvironment = false;
};

registry_string_payload registry_payload(const wchar_t* data, DWORD bytes, std::initializer_list<std::wstring> expected)
{
    const auto* first = reinterpret_cast<const BYTE*>(data);
    return {{first, first + bytes}, expected, expected.size() ? *expected.begin() : std::wstring{}};
}

std::vector<registry_string_scenario> registry_string_scenarios()
{
    constexpr wchar_t c_embeddedNull[] = L"abc\0def";
    constexpr wchar_t c_embeddedOddNonzeroTail[] = L"abc\0defA";
    constexpr wchar_t c_trailingNull[] = L"ab\0";
    constexpr DWORD c_orphanBytes = 1;
    constexpr DWORD c_embeddedOddBytes = sizeof(c_embeddedNull) - c_orphanBytes;
    const std::wstring c_embeddedExpected{c_embeddedNull, ARRAYSIZE(c_embeddedNull) - 1};
    const std::wstring c_trailingNullExpected{c_trailingNull, ARRAYSIZE(c_trailingNull) - 1};
    const auto single = [](const wchar_t* data, DWORD bytes, PCWSTR expected) {
        return registry_payload(data, bytes, {expected});
    };
    const auto scenario = [&](const char* name,
                              const wchar_t* data,
                              DWORD bytes,
                              const std::wstring& expectedBstr,
                              const wchar_t* multiData,
                              DWORD multiBytes,
                              std::initializer_list<std::wstring> multiExpected) {
        auto value = registry_string_scenario{
            name,
            {single(data, bytes, expectedBstr.c_str()),
             single(data, bytes, expectedBstr.c_str()),
             registry_payload(multiData, multiBytes, multiExpected)}};
        value.payload[0].ownedScalar = expectedBstr;
        value.payload[1].ownedScalar = expectedBstr;
        return value;
    };
    // Add one payload definition with expected scalar/MULTI outputs; each scenario covers REG_SZ, REG_EXPAND_SZ and REG_MULTI_SZ.
    // Normal/noexcept routes use explicit feature guards: only supported adapters compile, not every API in noexcept.
    // MULTI nulls separate elements; the current walker omits a string whose terminator is the range's last WCHAR.
    std::vector<registry_string_scenario> cases{
        scenario("empty-zero", L"", 0, L"", L"", 0, {L""}),
        scenario("empty-one-byte", L"", 1, L"", L"", 1, {L""}),
        scenario("orphan-nonzero-byte", L"A", c_orphanBytes, L"", L"A", c_orphanBytes, {L""}),
        scenario("empty-terminated", L"", 2, L"", L"\0", 4, {L""}),
        scenario("missing-terminator-even", L"abc", 6, L"abc", L"abc\0def", 14, {L"abc", L"def"}),
        scenario("missing-terminator-odd", L"abcx", 7, L"abc", L"abc\0defx", 15, {L"abc", L"def"}),
        scenario("embedded-null", c_embeddedNull, sizeof(c_embeddedNull), c_embeddedExpected, L"abc\0\0def\0", 20, {L"abc", L"", L"def"}),
        scenario("embedded-null-odd-zero-tail", c_embeddedNull, c_embeddedOddBytes, c_embeddedExpected, c_embeddedNull, c_embeddedOddBytes, {L"abc", L"def"}),
        scenario(
            "embedded-null-odd-nonzero-tail",
            c_embeddedOddNonzeroTail,
            c_embeddedOddBytes,
            c_embeddedExpected,
            c_embeddedOddNonzeroTail,
            c_embeddedOddBytes,
            {L"abc", L"def"}),
        scenario(
            "multiple-full-trailing-nulls", c_trailingNull, sizeof(c_trailingNull), c_trailingNullExpected, c_trailingNull, sizeof(c_trailingNull), {L"ab"}),
        scenario("aligned-null-trailing-byte", L"abc\0A", 9, L"abc", L"abc\0def\0A", 17, {L"abc", L"def"}),
        scenario("aligned-double-null-trailing-byte", L"abc\0A", 9, L"abc", L"abc\0def\0\0A", 19, {L"abc", L"def"}),
        scenario("stable-odd", L"ab\0x", 7, L"ab", L"abc\0def\0\0A", 19, {L"abc", L"def"}),
        scenario("value-growth-odd", L"abcdefghijklmnop", 29, L"abcdefghijklmn", L"abcd\0efgh\0ijklmnop", 29, {L"abcd", L"efgh", L"ijkl"})};
    cases.back().growValue = true;
    std::wstring large(4096, L'z');
    auto largeBytes = static_cast<DWORD>((large.size() + 1) * sizeof(wchar_t));
    cases.push_back({"large", {registry_payload(large.c_str(), largeBytes, {large}),
                              registry_payload(large.c_str(), largeBytes, {large}),
                              registry_payload(large.c_str(), largeBytes, {large})}});
    constexpr wchar_t token[] = L"%WIL_THREE_PARSER_TEST%";
    constexpr wchar_t oddToken[] = L"%WIL_THREE_PARSER_TEST%\0A";
    const std::wstring expanded(256, L'e');
    for (bool odd : {false, true})
    {
        cases.push_back({odd ? "environment-odd-trailing" : "environment-substitution",
                         {single(odd ? oddToken : token, odd ? sizeof(oddToken) - 3 : sizeof(token), token),
                          registry_payload(odd ? oddToken : token, odd ? sizeof(oddToken) - 3 : sizeof(token), {expanded}),
                          registry_payload(odd ? oddToken : token, odd ? sizeof(oddToken) - 3 : sizeof(token), {token})}});
    }
    cases.push_back({"environment-growth",
                     {single(token, sizeof(token), token), registry_payload(token, sizeof(token), {expanded}),
                      registry_payload(token, sizeof(token), {token})}, false, true});
    return cases;
}

// Process-local environment mutation and thread-local detours are restored before leaving each leaf.
template <typename Fn>
void with_registry_string_scenario(const registry_string_scenario& scenario, size_t parser, Fn read)
{
    const auto& payload = scenario.payload[parser];
    wil::unique_hkey key;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, key, wil::reg::key_access::readwrite));
    wchar_t oldEnvironment[512]{};
    ::SetLastError(ERROR_SUCCESS);
    const auto oldLength = ::GetEnvironmentVariableW(threeParserEnvironment, oldEnvironment, ARRAYSIZE(oldEnvironment));
    const bool hadEnvironment = oldLength != 0 || ::GetLastError() != ERROR_ENVVAR_NOT_FOUND;
    REQUIRE(oldLength < ARRAYSIZE(oldEnvironment));
    auto restoreEnvironment = wil::scope_exit([&] {
        ::SetEnvironmentVariableW(threeParserEnvironment, hadEnvironment ? oldEnvironment : nullptr);
    });
    const std::wstring expanded(256, L'e');
    REQUIRE(::SetEnvironmentVariableW(threeParserEnvironment, scenario.growEnvironment ? L"e" : expanded.c_str()));
    const DWORD initialBytes = scenario.growValue ? 23 : static_cast<DWORD>(payload.bytes.size());
    REQUIRE(ERROR_SUCCESS == set_value_exact_bytes(key.get(), threeParserValue, threeParserTypes[parser], payload.bytes, initialBytes));
    uint32_t calls = 0;
    uint32_t moreData = 0;
    bool mutationFailed = false;
    witest::detoured_thread_function<&::RegGetValueW> detour;
    REQUIRE_SUCCEEDED(detour.reset([&](HKEY hkey, PCWSTR subkey, PCWSTR name, DWORD flags, DWORD* type, void* data, DWORD* bytes) -> LSTATUS {
        // Bound nonconverging read retries so a regression fails rather than hangs.
        if (++calls > 16)
        {
            return ERROR_INVALID_DATA;
        }
        if (calls == 2 && scenario.growValue)
        {
            mutationFailed = ERROR_SUCCESS != set_value_exact_bytes(key.get(), threeParserValue, threeParserTypes[parser],
                                                                    payload.bytes, static_cast<DWORD>(payload.bytes.size()));
        }
        if (calls == 2 && scenario.growEnvironment)
        {
            mutationFailed = !::SetEnvironmentVariableW(threeParserEnvironment, expanded.c_str());
        }
        const auto status = ::RegGetValueW(hkey, subkey, name, flags, type, data, bytes);
        moreData += status == ERROR_MORE_DATA;
        return status;
    }));
    read(key.get(), payload);
    REQUIRE_FALSE(mutationFailed);
    REQUIRE(calls > 0);
    REQUIRE(calls <= 16);
    if (scenario.growValue || (scenario.growEnvironment && parser == 1))
    {
        REQUIRE(calls >= 3);
        REQUIRE(moreData > 0);
    }
}

void verify_registry_owned_string(PCWSTR data, const std::wstring& expected)
{
    // Establish nonnull and owned end-null (c_str()[size()] for wstring) before any unbounded read.
    // Bounded wcsnlen and end-null checks prove more than content equality alone.
    REQUIRE(data != nullptr);
    REQUIRE(data[expected.size()] == L'\0');
    REQUIRE(::wcsnlen(data, expected.size() + 1) == expected.size());
    REQUIRE(wmemcmp(data, expected.c_str(), expected.size()) == 0);
}

// Borrows a BSTR (or null for empty output); ownedScalar retains all counted UTF-16 characters.
void verify_registry_bstr(_In_opt_ BSTR data, const registry_string_payload& payload)
{
    const auto length = ::SysStringLen(data);
    REQUIRE(length == payload.ownedScalar.size());
    REQUIRE(::SysStringByteLen(data) == payload.ownedScalar.size() * sizeof(WCHAR));
    if (!data && payload.bytes.empty())
    {
        return;
    }
    REQUIRE(data != nullptr);
    REQUIRE(data[length] == L'\0');
    REQUIRE(wmemcmp(data, payload.ownedScalar.c_str(), length) == 0);
    REQUIRE(::wcsnlen(data, length + 1) == payload.expected.front().size());
}

template <typename T>
void verify_registry_string_result(const T& value, const registry_string_payload& payload)
{
    // A zero-byte successful read may leave pointer-based adapters null; no dereference is permitted then.
    if (!value && payload.bytes.empty())
    {
        REQUIRE(payload.expected.front().empty());
        return;
    }
    verify_registry_owned_string(value.get(), payload.expected.front());
}

#if defined(__WIL_OLEAUTO_H_)
void verify_registry_string_result(const wil::unique_bstr& value, const registry_string_payload& payload)
{
    verify_registry_bstr(value.get(), payload);
}
#endif
#if defined(__WIL_OLEAUTO_H_STL)
void verify_registry_string_result(const wil::shared_bstr& value, const registry_string_payload& payload)
{
    verify_registry_bstr(value.get(), payload);
}
#endif
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
void verify_registry_string_result(const std::wstring& value, const registry_string_payload& payload)
{
    REQUIRE(value.c_str()[value.size()] == L'\0');
    REQUIRE(value.size() == payload.expected.front().size());
    REQUIRE(value == payload.expected.front());
}
#endif

template <typename T, bool Named, bool PublicTry = false>
HRESULT registry_scalar_read(HKEY key, size_t parser, size_t route, T& value)
{
    if (route == 0 || route == 5)
    {
        return wil::reg::reg_view_details::reg_view_nothrow{key}.get_value(
            nullptr, threeParserValue, value, threeParserTypes[parser]);
    }
    if constexpr (Named)
    {
        if (route == 1)
        {
            return parser == 0 ? wil::reg::get_value_string_nothrow(key, threeParserValue, value) :
                                 wil::reg::get_value_expanded_string_nothrow(key, threeParserValue, value);
        }
        if (route == 2)
        {
            return wil::reg::get_value_nothrow(key, threeParserValue, value);
        }
    }
#if defined(WIL_ENABLE_EXCEPTIONS)
    if constexpr (!Named && std::is_same_v<T, std::wstring>)
    {
        if (route == 2)
        {
            return wil::reg::get_value_nothrow(key, threeParserValue, &value);
        }
    }
    if (route == 3)
    {
        if constexpr (Named || std::is_same_v<T, std::wstring>)
        {
            value = parser == 0 ? wil::reg::get_value_string<T>(key, threeParserValue) :
                                  wil::reg::get_value_expanded_string<T>(key, threeParserValue);
        }
        else
        {
            wil::reg::reg_view_details::reg_view{key}.get_value(nullptr, threeParserValue, value, threeParserTypes[parser]);
        }
    }
    if (route == 4)
    {
        auto result = wil::reg::reg_view_details::reg_view{key}.template try_get_value<T>(
            nullptr, threeParserValue, threeParserTypes[parser]);
        if (!result)
        {
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }
        value = std::move(*result);
    }
    if constexpr (PublicTry)
    {
        if (route == 6 || route == 7)
        {
            auto result = route == 7 ? wil::reg::try_get_value<T>(key, threeParserValue) :
                (parser == 0 ? wil::reg::try_get_value_string<T>(key, threeParserValue) :
                               wil::reg::try_get_value_expanded_string<T>(key, threeParserValue));
            if (!result)
            {
                return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
            }
            value = std::move(*result);
        }
    }
#endif
    return S_OK;
}

constexpr const char* registryScalarRoutes[]{"view", "named", "generic", "get", "view-try", "view-preallocated", "try", "generic-try"};

template <bool Named, bool PublicTry = false>
bool registry_scalar_route_supported(size_t parser, size_t route)
{
    if ((!Named && (route == 1 || (route == 2 && !PublicTry))) || (parser != 0 && (route == 2 || route == 7)) ||
        (!PublicTry && route >= 6))
    {
        return false;
    }
#if !defined(WIL_ENABLE_EXCEPTIONS)
    if (route == 3 || route == 4 || route >= 6)
    {
        return false;
    }
#endif
    return true;
}

template <typename T, bool Named = true, bool PublicTry = false>
void registry_scalar_scenarios(const registry_string_scenario& scenario, size_t parser, const char* adapter)
{
    for (size_t route = 0; route < ARRAYSIZE(registryScalarRoutes); ++route)
    {
        if (!registry_scalar_route_supported<Named, PublicTry>(parser, route))
        {
            continue;
        }
        DYNAMIC_SECTION(scenario.name << " / " << threeParserNames[parser] << " / " << adapter << " / " << registryScalarRoutes[route])
        {
            with_registry_string_scenario(scenario, parser, [&](HKEY key, const registry_string_payload& payload) {
                T value{};
                if (route == 5)
                {
                    REQUIRE_SUCCEEDED(wil::reg::reg_view_details::reg_value_type_info::resize_buffer_bytes(value, 8));
                }
                REQUIRE_SUCCEEDED((registry_scalar_read<T, Named, PublicTry>(key, parser, route, value)));
                verify_registry_string_result(value, payload);
            });
        }
    }
}

#if defined(__WIL_OBJBASE_H_)
void verify_registry_multi_result(
    const wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string>& value, const registry_string_payload& payload)
{
    REQUIRE(value.size() == payload.expected.size());
    if (!payload.expected.empty())
    {
        REQUIRE(value.get() != nullptr);
    }
    for (size_t i = 0; i < value.size(); ++i)
    {
        verify_registry_owned_string(value[i], payload.expected[i]);
    }
}
#endif
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
void verify_registry_multi_result(const std::vector<std::wstring>& value, const registry_string_payload& payload)
{
    REQUIRE(value.size() == payload.expected.size());
    for (size_t i = 0; i < value.size(); ++i)
    {
        REQUIRE(value[i].c_str()[value[i].size()] == L'\0');
        REQUIRE(value[i].size() == payload.expected[i].size());
        REQUIRE(value[i] == payload.expected[i]);
    }
}
#endif
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::three_parser_payloads
// Coverage: REG_SZ/EXPAND_SZ scalars and MULTI_SZ elements: empty, odd/missing/embedded nulls, large values,
// substitution and value/environment growth; verify contents, lengths and owned terminators.
// reg_view/named/generic nothrow reads use process heap, OLEAUTO BSTR and OBJBASE CoTaskMem, including raw BSTR;
// shared/wstring/vector and get/try routes require exceptions and their STL guards.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::three_parser_payloads", "[registry][registry-three-parser]")
{
    const auto cases = registry_string_scenarios();
    // Use unique DYNAMIC_SECTION identities by scenario/payload ID, type, adapter and route, not byte length (two can be 19).
    // Repeated SECTION identities in a loop can silently skip iterations.
    for (const auto& scenario : cases)
    {
        for (size_t parser = 0; parser < ARRAYSIZE(threeParserTypes); ++parser)
        {
            if (parser < 2)
            {
#if defined(__WIL_OLEAUTO_H_)
                registry_scalar_scenarios<wil::unique_bstr>(scenario, parser, "unique_bstr");
                for (size_t route = 0; route < (parser == 0 ? 5u : 2u); ++route)
                {
#if !defined(WIL_ENABLE_EXCEPTIONS)
                    if (route >= 3)
                    {
                        continue;
                    }
#endif
                    constexpr const char* routes[]{"view", "view-preallocated", "generic-nothrow", "generic-get", "generic-try"};
                    DYNAMIC_SECTION(scenario.name << " / " << threeParserNames[parser] << " / raw_bstr / " <<
                        routes[route])
                    {
                        with_registry_string_scenario(scenario, parser, [&](HKEY key, const registry_string_payload& payload) {
                            BSTR value = route == 1 ? ::SysAllocStringLen(L"old", 3) : nullptr;
                            auto cleanup = wil::scope_exit([&] { ::SysFreeString(value); });
                            if (route == 1)
                            {
                                REQUIRE(value != nullptr);
                            }
                            if (route < 2)
                            {
                                REQUIRE_SUCCEEDED(wil::reg::reg_view_details::reg_view_nothrow{key}.get_value(
                                    nullptr, threeParserValue, value, threeParserTypes[parser]));
                            }
                            else if (route == 2)
                            {
                                REQUIRE_SUCCEEDED(wil::reg::get_value_nothrow(key, threeParserValue, &value));
                            }
#if defined(WIL_ENABLE_EXCEPTIONS)
                            else if (route == 3)
                            {
                                value = wil::reg::get_value<BSTR>(key, threeParserValue);
                            }
                            else
                            {
                                auto result = wil::reg::try_get_value<BSTR>(key, threeParserValue);
                                REQUIRE(result.has_value());
                                value = *result;
                            }
#endif
                            verify_registry_bstr(value, payload);
                        });
                    }
                }
#endif
#if defined(__WIL_OLEAUTO_H_STL)
                registry_scalar_scenarios<wil::shared_bstr, true, true>(scenario, parser, "shared_bstr");
#endif
#if defined(__WIL_OBJBASE_H_)
                registry_scalar_scenarios<wil::unique_cotaskmem_string>(scenario, parser, "unique_cotaskmem");
#endif
#if defined(__WIL_OBJBASE_H_STL)
                registry_scalar_scenarios<wil::shared_cotaskmem_string, true, true>(scenario, parser, "shared_cotaskmem");
#endif
                registry_scalar_scenarios<wil::unique_process_heap_string, false>(scenario, parser, "process_heap");
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
                registry_scalar_scenarios<std::wstring, false, true>(scenario, parser, "wstring");
#endif
            }
            else
            {
#if defined(__WIL_OBJBASE_H_)
                for (bool generic : {false, true})
                {
                    DYNAMIC_SECTION(scenario.name << " / REG_MULTI_SZ / cotaskmem_array / " << (generic ? "generic" : "named"))
                    {
                        with_registry_string_scenario(scenario, parser, [&](HKEY key, const registry_string_payload& payload) {
                            wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> value;
                            REQUIRE_SUCCEEDED(generic ? wil::reg::get_value_nothrow(key, threeParserValue, value) :
                                                        wil::reg::get_value_multistring_nothrow(key, threeParserValue, value));
                            verify_registry_multi_result(value, payload);
                        });
                    }
                }
#endif
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
                for (size_t route = 0; route < 4; ++route)
                {
                    constexpr const char* routes[]{"get", "try", "generic-get", "generic-try"};
                    DYNAMIC_SECTION(scenario.name << " / REG_MULTI_SZ / wstring_vector / " << routes[route])
                    {
                        with_registry_string_scenario(scenario, parser, [&](HKEY key, const registry_string_payload& payload) {
                            std::vector<std::wstring> value;
                            if (route == 0)
                            {
                                value = wil::reg::get_value_multistring(key, threeParserValue);
                            }
                            else if (route == 2)
                            {
                                value = wil::reg::get_value<std::vector<std::wstring>>(key, threeParserValue);
                            }
                            else
                            {
                                auto result = route == 1 ? wil::reg::try_get_value_multistring(key, threeParserValue) :
                                                           wil::reg::try_get_value<std::vector<std::wstring>>(key, threeParserValue);
                                REQUIRE(result.has_value());
                                value = std::move(*result);
                            }
                            verify_registry_multi_result(value, payload);
                        });
                    }
                }
#endif
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::three_parser_raw_and_fixed
// Coverage: SZ/EXPAND/MULTI scenario byte extents match native RegGetValueW via OBJBASE CoTaskMem named/reg_view
// readers and exception-enabled STL byte-vector reg_view reads, including malformed data and growth retries.
// Fixed WCHAR named SZ/EXPAND and explicit reg_view arrays verify native bytes without allocation or growth retries.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::three_parser_raw_and_fixed", "[registry][registry-three-parser]")
{
    const auto cases = registry_string_scenarios();
    for (const auto& scenario : cases)
    {
        for (size_t parser = 0; parser < ARRAYSIZE(threeParserTypes); ++parser)
        {
            const auto verifyRaw = [&](HKEY key, const BYTE* bytes, size_t count) {
                std::array<BYTE, 16384> native{};
                DWORD nativeSize = static_cast<DWORD>(native.size());
                DWORD storedType = REG_NONE;
                REQUIRE(ERROR_SUCCESS == ::RegGetValueW(key, nullptr, threeParserValue,
                    wil::reg::reg_view_details::get_value_flags_from_value_type(threeParserTypes[parser]),
                    &storedType, native.data(), &nativeSize));
                REQUIRE(storedType == (parser == 1 ? REG_SZ : threeParserTypes[parser]));
                REQUIRE(count == nativeSize);
                if (count)
                {
                    REQUIRE(bytes != nullptr);
                    REQUIRE(memcmp(bytes, native.data(), count) == 0);
                }
            };
#if defined(__WIL_OBJBASE_H_)
            for (bool named : {false, true})
            {
                DYNAMIC_SECTION(scenario.name << " / " << threeParserNames[parser] << " / cotaskmem_bytes / " << (named ? "raw-named" : "raw-view"))
                {
                    with_registry_string_scenario(scenario, parser, [&](HKEY key, const registry_string_payload&) {
                        wil::unique_cotaskmem_array_ptr<BYTE> value;
                        REQUIRE_SUCCEEDED(named ? wil::reg::get_value_binary_nothrow(key, threeParserValue, threeParserTypes[parser], value) :
                            wil::reg::reg_view_details::reg_view_nothrow{key}.get_value(nullptr, threeParserValue, value, threeParserTypes[parser]));
                        verifyRaw(key, value.get(), value.size());
                    });
                }
            }
#endif
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
            DYNAMIC_SECTION(scenario.name << " / " << threeParserNames[parser] << " / vector_bytes / raw-view")
            {
                with_registry_string_scenario(scenario, parser, [&](HKEY key, const registry_string_payload&) {
                    std::vector<BYTE> value;
                    REQUIRE_SUCCEEDED(wil::reg::reg_view_details::reg_view_nothrow{key}.get_value(
                        nullptr, threeParserValue, value, threeParserTypes[parser]));
                    verifyRaw(key, value.data(), value.size());
                });
            }
#endif
            // Fixed arrays delegate native termination, and never allocate/retry after MORE_DATA.
            if (!scenario.growValue && !scenario.growEnvironment)
            {
                for (bool named : {false, true})
                {
                    if (named && parser == 2)
                    {
                        continue;
                    }
                    DYNAMIC_SECTION(scenario.name << " / " << threeParserNames[parser] << " / fixed_array / " << (named ? "named" : "view"))
                    {
                        with_registry_string_scenario(scenario, parser, [&](HKEY key, const registry_string_payload& payload) {
                            wchar_t value[8192]{};
                            DWORD requiredBytes = 0;
                            REQUIRE_SUCCEEDED(named ?
                                (parser == 0 ? wil::reg::get_value_string_nothrow(key, threeParserValue, value, &requiredBytes) :
                                               wil::reg::get_value_expanded_string_nothrow(key, threeParserValue, value, &requiredBytes)) :
                                wil::reg::reg_view_details::reg_view_nothrow{key}.get_value_char_array(
                                    nullptr, threeParserValue, value, threeParserTypes[parser], &requiredBytes));
                            REQUIRE(requiredBytes <= sizeof(value));
                            verifyRaw(key, reinterpret_cast<const BYTE*>(value), requiredBytes);
                            if (parser < 2)
                            {
                                verify_registry_owned_string(value, payload.expected.front());
                            }
                        });
                    }
                }
            }
        }
    }
}

namespace
{
// Valid output owners and preallocation stay outside the owned-thread read/resize telemetry phase.
// Count/refuse active overflow allocations independently of payload-OOM arming; otherwise zero attempts could be vacuous.
template <typename T, bool Named = true, bool PublicTry = false>
void registry_scalar_failures(size_t parser, const char* adapter)
{
    for (const char* failure : {"payload-oom", "extreme-overflow", "retry-error"})
    {
        for (size_t route = 0; route < ARRAYSIZE(registryScalarRoutes); ++route)
        {
            if (!registry_scalar_route_supported<Named, PublicTry>(parser, route))
            {
                continue;
            }
            DYNAMIC_SECTION(failure << " / " << threeParserNames[parser] << " / " << adapter << " / " << registryScalarRoutes[route])
            {
                const bool overflow = strcmp(failure, "extreme-overflow") == 0;
                const bool retryError = strcmp(failure, "retry-error") == 0;
                size_t readCalls = 0;
                size_t allocations = 0;
                bool active = false;
                bool readPhase = false;
                bool armed = false;
                bool injected = false;
                witest::detoured_thread_function<&::RegGetValueW> read;
                REQUIRE_SUCCEEDED(read.reset([&](HKEY, PCWSTR, PCWSTR, DWORD, DWORD*, void*, DWORD* bytes) -> LSTATUS {
                    readPhase = true;
                    ++readCalls;
                    if (readCalls > 16)
                    {
                        return ERROR_INVALID_DATA;
                    }
                    *bytes = overflow ? MAXDWORD : 32;
                    armed = !overflow && !retryError;
                    return retryError && readCalls > 1 ? ERROR_ACCESS_DENIED : ERROR_MORE_DATA;
                }));
                // Hook WIL's allocation boundary, not HeapAlloc (also used by other threads' TLS initialization).
                witest::detoured_thread_function<&wil::details::heap_allocator::allocate> heap;
                REQUIRE_SUCCEEDED(heap.reset([&](size_t bytes) -> void* {
                    if (active && ((overflow && readPhase) || (armed && !injected)))
                    {
                        injected = true;
                        ++allocations;
                        return nullptr;
                    }
                    return wil::details::heap_allocator::allocate(bytes);
                }));
#if defined(__WIL_OBJBASE_H_)
                witest::detoured_thread_function<&::CoTaskMemAlloc> task;
                REQUIRE_SUCCEEDED(task.reset([&](size_t bytes) -> void* {
                    if (active && ((overflow && readPhase) || (armed && !injected)))
                    {
                        injected = true;
                        ++allocations;
                        return nullptr;
                    }
                    return ::CoTaskMemAlloc(bytes);
                }));
#endif
#if defined(__WIL_OLEAUTO_H_)
                witest::detoured_thread_function<&::SysAllocStringLen> bstr;
                REQUIRE_SUCCEEDED(bstr.reset([&](const OLECHAR* data, UINT length) -> BSTR {
                    if (active && ((overflow && readPhase) || (armed && !injected)))
                    {
                        injected = true;
                        ++allocations;
                        return nullptr;
                    }
                    return ::SysAllocStringLen(data, length);
                }));
#endif
#if defined(WIL_ENABLE_EXCEPTIONS)
                witest::detoured_thread_function<static_cast<void* (*)(size_t)>(&::operator new)> stl;
                REQUIRE_SUCCEEDED(stl.reset([&](size_t bytes) -> void* {
                    if (active && ((overflow && readPhase) || (armed && !injected)))
                    {
                        injected = true;
                        ++allocations;
                        throw std::bad_alloc();
                    }
                    return ::operator new(bytes);
                }));
#endif
                if (overflow)
                {
                    // A short resize probe proves the hook works before checking absence of overflow allocations.
                    T probe{};
                    HRESULT probeHr = S_OK;
                    active = readPhase = true;
#if defined(WIL_ENABLE_EXCEPTIONS)
                    const auto probeException = wil::ResultFromException([&] {
                        probeHr = wil::reg::reg_view_details::reg_value_type_info::resize_buffer_bytes(probe, 512);
                    });
                    if (FAILED(probeException))
                    {
                        probeHr = probeException;
                    }
#else
                    probeHr = wil::reg::reg_view_details::reg_value_type_info::resize_buffer_bytes(probe, 512);
#endif
                    active = false;
                    REQUIRE(probeHr == E_OUTOFMEMORY);
                    REQUIRE(allocations == 1);
                    REQUIRE(injected);
                    REQUIRE_FALSE(armed);
                    allocations = 0;
                    injected = false;
                    readPhase = false;
                }
                T value{};
                if (route == 5)
                {
                    REQUIRE_SUCCEEDED(wil::reg::reg_view_details::reg_value_type_info::resize_buffer_bytes(value, 8));
                }
                const auto oldBuffer = wil::reg::reg_view_details::reg_value_type_info::get_buffer(value);
#if defined(__WIL_OLEAUTO_H_)
                auto cleanup = wil::scope_exit([&] {
                    if constexpr (std::is_same_v<T, BSTR>)
                    {
                        ::SysFreeString(value);
                    }
                });
#endif
                active = true;
                HRESULT hr = S_OK;
#if defined(WIL_ENABLE_EXCEPTIONS)
                const auto exceptionHr = wil::ResultFromException([&] {
                    hr = registry_scalar_read<T, Named, PublicTry>(HKEY_CURRENT_USER, parser, route, value);
                });
                if (FAILED(exceptionHr))
                {
                    hr = exceptionHr;
                }
#else
                hr = registry_scalar_read<T, Named, PublicTry>(HKEY_CURRENT_USER, parser, route, value);
#endif
                active = false;
                REQUIRE(hr == (retryError ? HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) :
                              overflow ? HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) : E_OUTOFMEMORY));
                REQUIRE(readCalls == (retryError ? 2u : 1u));
                if (overflow)
                {
                    REQUIRE(allocations == 0);
                }
                else if (!retryError)
                {
                    REQUIRE(allocations == 1);
                }
                if (route == 5 && !retryError)
                {
                    REQUIRE(wil::reg::reg_view_details::reg_value_type_info::get_buffer(value) == oldBuffer);
                }
            }
        }
    }
}
template <typename T, bool Named = true, bool PublicTry = false>
void registry_scalar_type_rejection(size_t parser, const char* adapter)
{
    wil::unique_hkey key;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, key, wil::reg::key_access::readwrite));
    for (DWORD stored : {REG_DWORD, REG_BINARY})
    {
        for (size_t route = 0; route < ARRAYSIZE(registryScalarRoutes); ++route)
        {
            if (!registry_scalar_route_supported<Named, PublicTry>(parser, route))
            {
                continue;
            }
            DYNAMIC_SECTION("type-mismatch-" << (stored == REG_DWORD ? "dword" : "binary") << " / " <<
                threeParserNames[parser] << " / " << adapter << " / " << registryScalarRoutes[route])
            {
                constexpr DWORD data = 123;
                REQUIRE(ERROR_SUCCESS == ::RegSetValueExW(key.get(), threeParserValue, 0, stored,
                    reinterpret_cast<const BYTE*>(&data), sizeof(data)));
                T value{};
#if defined(__WIL_OLEAUTO_H_)
                auto cleanup = wil::scope_exit([&] {
                    if constexpr (std::is_same_v<T, BSTR>)
                    {
                        ::SysFreeString(value);
                    }
                });
#endif
                if (route == 5)
                {
                    REQUIRE_SUCCEEDED(wil::reg::reg_view_details::reg_value_type_info::resize_buffer_bytes(value, 8));
                }
                HRESULT hr = S_OK;
#if defined(WIL_ENABLE_EXCEPTIONS)
                const auto exceptionHr = wil::ResultFromException([&] {
                    hr = registry_scalar_read<T, Named, PublicTry>(key.get(), parser, route, value);
                });
                if (FAILED(exceptionHr))
                {
                    hr = exceptionHr;
                }
#else
                hr = registry_scalar_read<T, Named, PublicTry>(key.get(), parser, route, value);
#endif
                REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
            }
        }
    }
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::three_parser_failure_paths
// Coverage: SZ/EXPAND scalar reg_view/named/generic reads reject payload OOM, rounded-size overflow and denied
// read retries; calibrated allocator hooks verify overflow skips allocation and failed growth preserves owners.
// OBJBASE-gated MULTI parsers and raw SZ/EXPAND/MULTI byte readers reject OOM or denied retries.
// OLEAUTO/OBJBASE gate owners; shared/wstring and get/try routes require exceptions and their STL guards.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::three_parser_failure_paths", "[registry][registry-three-parser]")
{
    for (size_t parser = 0; parser < 2; ++parser)
    {
#if defined(__WIL_OLEAUTO_H_)
        registry_scalar_failures<wil::unique_bstr>(parser, "unique_bstr");
        registry_scalar_failures<BSTR, false>(parser, "raw_bstr");
#endif
#if defined(__WIL_OLEAUTO_H_STL)
        registry_scalar_failures<wil::shared_bstr, true, true>(parser, "shared_bstr");
#endif
#if defined(__WIL_OBJBASE_H_)
        registry_scalar_failures<wil::unique_cotaskmem_string>(parser, "unique_cotaskmem");
#endif
#if defined(__WIL_OBJBASE_H_STL)
        registry_scalar_failures<wil::shared_cotaskmem_string, true, true>(parser, "shared_cotaskmem");
#endif
        registry_scalar_failures<wil::unique_process_heap_string, false>(parser, "process_heap");
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
        registry_scalar_failures<std::wstring, false, true>(parser, "wstring");
#endif
    }
#if defined(__WIL_OBJBASE_H_)
    for (const char* failure : {"payload-oom", "extreme-overflow", "retry-error"})
    {
        for (size_t parser = 0; parser < ARRAYSIZE(threeParserTypes); ++parser)
        {
            for (size_t route = 0; route < (parser == 2 ? 4u : 2u); ++route)
            {
                const bool parsed = route >= 2;
                const bool generic = route % 2 != 0;
                const char* name = parsed ? (generic ? "generic" : "named") : (generic ? "raw-view" : "raw-named");
                DYNAMIC_SECTION(failure << " / " << threeParserNames[parser] << " / " <<
                    (parsed ? "cotaskmem_array" : "cotaskmem_bytes") << " / " << name)
                {
                    const bool extreme = strcmp(failure, "extreme-overflow") == 0;
                    const bool retry = strcmp(failure, "retry-error") == 0;
                    size_t reads = 0;
                    size_t allocations = 0;
                    size_t requestedBytes = 0;
                    bool active = false;
                    witest::detoured_thread_function<&::RegGetValueW> read;
                    REQUIRE_SUCCEEDED(read.reset([&](HKEY, PCWSTR, PCWSTR, DWORD, DWORD*, void*, DWORD* bytes) -> LSTATUS {
                        *bytes = extreme ? MAXDWORD : 32;
                        return ++reads > 1 && retry ? ERROR_ACCESS_DENIED : ERROR_MORE_DATA;
                    }));
                    witest::detoured_thread_function<&::CoTaskMemAlloc> task;
                    REQUIRE_SUCCEEDED(task.reset([&](size_t bytes) -> void* {
                        if (active)
                        {
                            ++allocations;
                            requestedBytes = bytes;
                            if (!retry || bytes >= 16384)
                            {
                                return nullptr;
                            }
                        }
                        return ::CoTaskMemAlloc(bytes);
                    }));
                    wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> value;
                    wil::unique_cotaskmem_array_ptr<BYTE> raw;
                    active = true;
                    const auto hr = parsed ?
                        (generic ? wil::reg::get_value_nothrow(HKEY_CURRENT_USER, threeParserValue, value) :
                                   wil::reg::get_value_multistring_nothrow(HKEY_CURRENT_USER, threeParserValue, value)) :
                        (generic ? wil::reg::reg_view_details::reg_view_nothrow{HKEY_CURRENT_USER}.get_value(
                                       nullptr, threeParserValue, raw, threeParserTypes[parser]) :
                                   wil::reg::get_value_binary_nothrow(HKEY_CURRENT_USER, threeParserValue, threeParserTypes[parser], raw));
                    active = false;
                    REQUIRE(hr == (retry ? HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) : E_OUTOFMEMORY));
                    REQUIRE(reads == (retry ? 2u : 1u));
                    REQUIRE(allocations == 1);
                    REQUIRE(requestedBytes == (extreme ? MAXDWORD : 32u));
                    REQUIRE(value.empty());
                }
            }
        }
    }
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::three_parser_stored_types
// Coverage: SZ/EXPAND scalar and MULTI parsers reject stored DWORD/BINARY through supported named/generic/view
// routes; explicit process-heap/fixed-array views check accepted SZ/EXPAND/MULTI combinations and mismatches.
// Explicit ANY reads BINARY as raw scalar bytes, not typed parsing; OBJBASE/OLEAUTO gate owning adapters,
// while shared/STL and get/try routes require exceptions.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::three_parser_stored_types", "[registry][registry-three-parser]")
{
    for (size_t parser = 0; parser < 2; ++parser)
    {
#if defined(__WIL_OLEAUTO_H_)
        registry_scalar_type_rejection<wil::unique_bstr>(parser, "unique_bstr");
        registry_scalar_type_rejection<BSTR, false>(parser, "raw_bstr");
#endif
#if defined(__WIL_OLEAUTO_H_STL)
        registry_scalar_type_rejection<wil::shared_bstr, true, true>(parser, "shared_bstr");
#endif
#if defined(__WIL_OBJBASE_H_)
        registry_scalar_type_rejection<wil::unique_cotaskmem_string>(parser, "unique_cotaskmem");
#endif
#if defined(__WIL_OBJBASE_H_STL)
        registry_scalar_type_rejection<wil::shared_cotaskmem_string, true, true>(parser, "shared_cotaskmem");
#endif
        registry_scalar_type_rejection<wil::unique_process_heap_string, false>(parser, "process_heap");
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
        registry_scalar_type_rejection<std::wstring, false, true>(parser, "wstring");
#endif
    }
    wil::unique_hkey key;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, key, wil::reg::key_access::readwrite));
    for (DWORD stored : {REG_DWORD, REG_BINARY})
    {
        for (size_t route = 0; route < 6; ++route)
        {
#if !defined(WIL_ENABLE_EXCEPTIONS)
            if (route >= 2)
            {
                continue;
            }
#endif
            const char* adapter = route < 2 ? "cotaskmem_array" : "wstring_vector";
            constexpr const char* routes[]{"named", "generic", "get", "try", "generic-get", "generic-try"};
            DYNAMIC_SECTION("type-mismatch-" << (stored == REG_DWORD ? "dword" : "binary") <<
                " / REG_MULTI_SZ / " << adapter << " / " << routes[route])
            {
                constexpr DWORD data = 123;
                REQUIRE(ERROR_SUCCESS == ::RegSetValueExW(key.get(), threeParserValue, 0, stored,
                    reinterpret_cast<const BYTE*>(&data), sizeof(data)));
                HRESULT hr = S_OK;
#if defined(__WIL_OBJBASE_H_)
                if (route < 2)
                {
                    wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> value;
                    hr = route == 0 ? wil::reg::get_value_multistring_nothrow(key.get(), threeParserValue, value) :
                                      wil::reg::get_value_nothrow(key.get(), threeParserValue, value);
                }
#endif
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
                if (route >= 2)
                {
                    hr = wil::ResultFromException([&] {
                        if (route == 2)
                        {
                            wil::reg::get_value_multistring(key.get(), threeParserValue);
                        }
                        else if (route == 3)
                        {
                            wil::reg::try_get_value_multistring(key.get(), threeParserValue);
                        }
                        else if (route == 4)
                        {
                            wil::reg::get_value<std::vector<std::wstring>>(key.get(), threeParserValue);
                        }
                        else
                        {
                            wil::reg::try_get_value<std::vector<std::wstring>>(key.get(), threeParserValue);
                        }
                    });
                }
#endif
                REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
            }
        }
    }
    for (DWORD stored : {REG_SZ, REG_EXPAND_SZ, REG_MULTI_SZ, REG_DWORD, REG_BINARY})
    {
        for (size_t parser = 0; parser < ARRAYSIZE(threeParserTypes); ++parser)
        {
            DYNAMIC_SECTION("stored=" << stored << " / " << threeParserNames[parser] << " / typed-rejection")
            {
                constexpr wchar_t data[] = L"abc\0";
                REQUIRE(ERROR_SUCCESS == ::RegSetValueExW(key.get(), threeParserValue, 0, stored,
                    reinterpret_cast<const BYTE*>(data), stored == REG_DWORD ? sizeof(DWORD) : sizeof(data)));
                const bool supported = parser < 2 ? stored == REG_SZ || stored == REG_EXPAND_SZ : stored == REG_MULTI_SZ;
                const auto expected = supported ? S_OK : HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE);
                wil::unique_process_heap_string scalar;
                REQUIRE(wil::reg::reg_view_details::reg_view_nothrow{key.get()}.get_value(
                    nullptr, threeParserValue, scalar, threeParserTypes[parser]) == expected);
                if (supported && parser < 2)
                {
                    verify_registry_owned_string(scalar.get(), L"abc");
                }
                wchar_t fixed[16]{};
                DWORD required = 0;
                REQUIRE(wil::reg::reg_view_details::reg_view_nothrow{key.get()}.get_value_char_array(
                    nullptr, threeParserValue, fixed, threeParserTypes[parser], &required) == expected);
#if defined(__WIL_OBJBASE_H_)
                if (parser == 2)
                {
                    wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> multi;
                    REQUIRE(wil::reg::get_value_multistring_nothrow(key.get(), threeParserValue, multi) == expected);
                    if (supported)
                    {
                        REQUIRE(multi.size() == 1);
                        verify_registry_owned_string(multi[0], L"abc");
                    }
                }
#endif
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
                if (parser == 2)
                {
                    HRESULT hr = S_OK;
                    const auto exceptionHr = wil::ResultFromException([&] {
                        const auto result = wil::reg::try_get_value_multistring(key.get(), threeParserValue);
                        if (!result)
                        {
                            hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
                        }
                    });
                    REQUIRE((FAILED(exceptionHr) ? exceptionHr : hr) == expected);
                }
#endif
            }
        }
    }
    SECTION("explicit ANY is raw selection, not typed rejection")
    {
        constexpr wchar_t binary[] = L"abc";
        REQUIRE(ERROR_SUCCESS == ::RegSetValueExW(key.get(), threeParserValue, 0, REG_BINARY,
            reinterpret_cast<const BYTE*>(binary), sizeof(binary)));
        wil::unique_process_heap_string value;
        REQUIRE_SUCCEEDED(wil::reg::reg_view_details::reg_view_nothrow{key.get()}.get_value(
            nullptr, threeParserValue, value, RRF_RT_ANY | RRF_NOEXPAND));
        verify_registry_owned_string(value.get(), L"abc");
    }
}

#if defined(WIL_ENABLE_EXCEPTIONS) && defined(__WIL_OLEAUTO_H_STL) && defined(__WIL_OBJBASE_H_STL)
namespace
{
template <typename T>
void registry_shared_parser_controlblock_oom(size_t parser, const char* adapter)
{
    for (size_t route = 0; route < ARRAYSIZE(registryScalarRoutes); ++route)
    {
        if (!registry_scalar_route_supported<true, true>(parser, route))
        {
            continue;
        }
        // Named literal BSTR uses the existing control block's raw BSTR slot; its initial addressof OOM has a separate test.
        if (std::is_same_v<T, wil::shared_bstr> && parser == 0 && route == 1)
        {
            continue;
        }
        DYNAMIC_SECTION("controlblock-oom / " << threeParserNames[parser] << " / " << adapter << " / " << registryScalarRoutes[route])
        {
            T value{};
            if (route == 5)
            {
                REQUIRE_SUCCEEDED(wil::reg::reg_view_details::reg_value_type_info::resize_buffer_bytes(value, 8));
            }
            auto alias = value;
            const auto old = value.get();
            bool active = false;
            bool armed = false;
            size_t failures = 0;
            size_t frees = 0;
            void* payload = nullptr;
            bool recordedPayload = false;
            witest::detoured_thread_function<static_cast<void* (*)(size_t)>(&::operator new)> control;
            REQUIRE_SUCCEEDED(control.reset([&](size_t bytes) -> void* {
                if (armed)
                {
                    armed = false;
                    ++failures;
                    throw std::bad_alloc();
                }
                return ::operator new(bytes);
            }));
            witest::detoured_thread_function<&::CoTaskMemAlloc> task;
            REQUIRE_SUCCEEDED(task.reset([&](size_t bytes) -> void* {
                auto allocated = ::CoTaskMemAlloc(bytes);
                if (active && !recordedPayload)
                {
                    recordedPayload = true;
                    payload = allocated;
                    armed = allocated != nullptr;
                }
                return allocated;
            }));
            witest::detoured_thread_function<&::SysAllocStringLen> bstr;
            REQUIRE_SUCCEEDED(bstr.reset([&](const OLECHAR* data, UINT length) -> BSTR {
                auto allocated = ::SysAllocStringLen(data, length);
                if (active && !recordedPayload)
                {
                    recordedPayload = true;
                    payload = allocated;
                    armed = allocated != nullptr;
                }
                return allocated;
            }));
            witest::detoured_thread_function<&::CoTaskMemFree> taskFree;
            REQUIRE_SUCCEEDED(taskFree.reset([&](void* ptr) {
                if (ptr && ptr == payload)
                {
                    ++frees;
                    payload = nullptr;
                }
                ::CoTaskMemFree(ptr);
            }));
            witest::detoured_thread_function<&::SysFreeString> bstrFree;
            REQUIRE_SUCCEEDED(bstrFree.reset([&](BSTR ptr) {
                if (ptr && ptr == payload)
                {
                    ++frees;
                    payload = nullptr;
                }
                ::SysFreeString(ptr);
            }));
            witest::detoured_thread_function<&::RegGetValueW> read;
            constexpr wchar_t data[] = L"replacement value";
            registry_allocation_value(read, data, sizeof(data));
            HRESULT hr = S_OK;
            active = true;
            const auto exceptionHr = wil::ResultFromException([&] {
                hr = registry_scalar_read<T, true, true>(HKEY_CURRENT_USER, parser, route, value);
            });
            active = false;
            REQUIRE((FAILED(exceptionHr) ? exceptionHr : hr) == E_OUTOFMEMORY);
            REQUIRE(failures == 1);
            REQUIRE(frees == 1);
            REQUIRE(value.get() == old);
            REQUIRE(alias.get() == old);
        }
    }
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::three_parser_shared_controlblock
// Coverage: Exception-enabled only, with OLEAUTO/OBJBASE shared adapters: SZ/EXPAND shared BSTR/CoTaskMem
// reg_view, named/generic and get/try reads map injected control-block bad_alloc to OOM, free replacements
// and preserve old owners/aliases; named literal BSTR's separate initial addressof path is excluded.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::three_parser_shared_controlblock", "[registry][registry-three-parser]")
{
    for (size_t parser = 0; parser < 2; ++parser)
    {
        registry_shared_parser_controlblock_oom<wil::shared_bstr>(parser, "shared_bstr");
        registry_shared_parser_controlblock_oom<wil::shared_cotaskmem_string>(parser, "shared_cotaskmem");
    }
}
#endif

#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::three_parser_vector_failures
// Coverage: Exception-enabled STL only: raw byte-vector reg_view reads of SZ/EXPAND/MULTI and MULTI vector
// named/generic get/try reads reject payload OOM and denied retries with empty parsed output.
// Extreme byte requests distinguish preallocation capacity rejection from representable, injected allocation OOM.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::three_parser_vector_failures", "[registry][registry-three-parser]")
{
    // Capacity rejection precedes allocation; only representable MAXDWORD requests exercise injected payload OOM.
    for (const char* failure : {"payload-oom", "extreme-overflow", "retry-error"})
    {
        for (size_t parser = 0; parser < ARRAYSIZE(threeParserTypes); ++parser)
        {
            for (size_t route = 0; route < 5; ++route)
            {
                if (parser != 2 && route != 4)
                {
                    continue;
                }
                constexpr const char* routes[]{"get", "try", "generic-get", "generic-try", "raw-view"};
                const char* adapter = route == 4 ? "vector_bytes" : "wstring_vector";
                DYNAMIC_SECTION(failure << " / " << threeParserNames[parser] << " / " << adapter << " / " << routes[route])
                {
                    const bool extreme = strcmp(failure, "extreme-overflow") == 0;
                    const bool retry = strcmp(failure, "retry-error") == 0;
                    const bool withinCapacity = static_cast<size_t>(MAXDWORD) <= std::vector<BYTE>{}.max_size();
                    bool active = false;
                    bool armed = false;
                    size_t reads = 0;
                    size_t failures = 0;
                    size_t refusedBytes = 0;
                    witest::detoured_thread_function<&::RegGetValueW> read;
                    REQUIRE_SUCCEEDED(read.reset([&](HKEY, PCWSTR, PCWSTR, DWORD, DWORD*, void*, DWORD* bytes) -> LSTATUS {
                        if (++reads > 16)
                        {
                            return ERROR_INVALID_DATA;
                        }
                        *bytes = extreme ? MAXDWORD : 32;
                        armed = !retry && (!extreme || withinCapacity);
                        return reads > 1 && retry ? ERROR_ACCESS_DENIED : ERROR_MORE_DATA;
                    }));
                    witest::detoured_thread_function<static_cast<void* (*)(size_t)>(&::operator new)> allocate;
                    REQUIRE_SUCCEEDED(allocate.reset([&](size_t bytes) -> void* {
                        if (active && (armed || (extreme && bytes >= static_cast<size_t>(MAXDWORD))))
                        {
                            armed = false;
                            ++failures;
                            refusedBytes = bytes;
                            throw std::bad_alloc();
                        }
                        return ::operator new(bytes);
                    }));
                    std::vector<BYTE> raw;
                    std::vector<std::wstring> strings;
                    HRESULT hr = S_OK;
                    active = true;
                    const auto exceptionHr = wil::ResultFromException([&] {
                        if (route == 4)
                        {
                            hr = wil::reg::reg_view_details::reg_view_nothrow{HKEY_CURRENT_USER}.get_value(
                                nullptr, threeParserValue, raw, threeParserTypes[parser]);
                        }
                        else if (route == 0 || route == 2)
                        {
                            strings = route == 0 ? wil::reg::get_value_multistring(HKEY_CURRENT_USER, threeParserValue) :
                                wil::reg::get_value<std::vector<std::wstring>>(HKEY_CURRENT_USER, threeParserValue);
                        }
                        else
                        {
                            auto result = route == 1 ? wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, threeParserValue) :
                                wil::reg::try_get_value<std::vector<std::wstring>>(HKEY_CURRENT_USER, threeParserValue);
                            if (!result)
                            {
                                hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
                            }
                        }
                    });
                    active = false;
                    REQUIRE((FAILED(exceptionHr) ? exceptionHr : hr) ==
                        (retry ? HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) :
                         extreme && !withinCapacity ? HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION) : E_OUTOFMEMORY));
                    REQUIRE(failures == (retry || (extreme && !withinCapacity) ? 0u : 1u));
                    if (extreme && withinCapacity)
                    {
                        // The STL allocator adds alignment/debug bookkeeping to the requested byte extent.
                        REQUIRE(refusedBytes >= MAXDWORD);
                        REQUIRE(refusedBytes - MAXDWORD < 1024);
                    }
                    REQUIRE(reads == (retry ? 2u : 1u));
                    REQUIRE(strings.empty());
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::three_parser_vector_parse_failures
// Coverage: Exception-enabled STL only: MULTI vector named/generic get/try reads inject post-read OOM at vector
// storage, first/middle/last long string storage and empty-element vector storage, calibrated by returned pointers.
// Failures produce no partial or missing-value result, free all storage and stop later calibrated parse allocations.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::three_parser_vector_parse_failures", "[registry][registry-three-parser]")
{
    // Post-read vector/element OOM is distinct from raw-buffer OOM; failure must leave empty output and unwind all storage.
    constexpr const char* stages[]{"element-allocation-array", "element-allocation-first",
        "element-allocation-middle", "element-allocation-last", "element-allocation-empty-array"};
    constexpr const char* routes[]{"get", "try", "generic-get", "generic-try"};
    constexpr size_t lengths[]{128, 256, 512};
    std::vector<wchar_t> payload;
    std::vector<wchar_t> firstPayload(lengths[0], L'a');
    firstPayload.insert(firstPayload.end(), 2, L'\0');
    for (size_t element = 0; element < ARRAYSIZE(lengths); ++element)
    {
        payload.insert(payload.end(), lengths[element], static_cast<wchar_t>(L'a' + element));
        payload.push_back(L'\0');
    }
    payload.push_back(L'\0');
    constexpr wchar_t emptyPayload[]{L'\0', L'\0'};

    for (size_t stage = 0; stage < ARRAYSIZE(stages); ++stage)
    {
        for (size_t route = 0; route < ARRAYSIZE(routes); ++route)
        {
            DYNAMIC_SECTION(stages[stage] << " / REG_MULTI_SZ / wstring_vector / " << routes[route])
            {
                struct allocation_record
                {
                    void* pointer;
                    size_t bytes;
                    bool afterRead;
                    bool freed;
                };
                std::array<allocation_record, 64> records{};
                size_t recordCount = 0;
                bool recordOverflow = false;
                bool active = false;
                bool inject = false;
                bool missing = false;
                bool wrongFlags = false;
                size_t reads = 0;
                size_t successfulReads = 0;
                size_t failures = 0;
                size_t allocationsAfterFailure = 0;
                std::array<size_t, 4> allocationBytes{};
                std::array<size_t, 4> stageAttempts{};
                const wchar_t* source = firstPayload.data();
                DWORD sourceBytes = static_cast<DWORD>(firstPayload.size() * sizeof(wchar_t));
                witest::detoured_thread_function<&::RegGetValueW> read;
                REQUIRE_SUCCEEDED(read.reset([&](HKEY, PCWSTR, PCWSTR, DWORD flags, DWORD*, void* data, DWORD* bytes) -> LSTATUS {
                    wrongFlags |= !(flags & RRF_RT_REG_MULTI_SZ);
                    if (++reads > 16)
                    {
                        return ERROR_INVALID_DATA;
                    }
                    const auto capacity = *bytes;
                    *bytes = sourceBytes;
                    // Match the real RegGetValueW: a size-only query (null data) succeeds.
                    if (!data)
                    {
                        return ERROR_SUCCESS;
                    }
                    if (capacity < sourceBytes)
                    {
                        return ERROR_MORE_DATA;
                    }
                    memcpy(data, source, sourceBytes);
                    ++successfulReads;
                    return ERROR_SUCCESS;
                }));
                // Capture actual public-read allocations without allocating telemetry storage on the owned thread.
                witest::detoured_thread_function<static_cast<void* (*)(size_t)>(&::operator new)> allocate;
                REQUIRE_SUCCEEDED(allocate.reset([&](size_t bytes) -> void* {
                    if (active && inject && successfulReads)
                    {
                        for (size_t index = 0; index < allocationBytes.size(); ++index)
                        {
                            if (bytes == allocationBytes[index])
                            {
                                ++stageAttempts[index];
                                allocationsAfterFailure += failures != 0;
                            }
                        }
                        if (!failures && bytes == allocationBytes[stage == 4 ? 0 : stage])
                        {
                            ++failures;
                            throw std::bad_alloc();
                        }
                    }
                    void* const pointer = ::operator new(bytes);
                    if (active)
                    {
                        if (recordCount < records.size())
                        {
                            records[recordCount++] = {pointer, bytes, successfulReads != 0, false};
                        }
                        else
                        {
                            recordOverflow = true;
                        }
                    }
                    return pointer;
                }));
                const auto markFreed = [&](void* pointer) {
                    if (active)
                    {
                        for (size_t index = recordCount; index > 0; --index)
                        {
                            auto& record = records[index - 1];
                            if (record.pointer == pointer && !record.freed)
                            {
                                record.freed = true;
                                break;
                            }
                        }
                    }
                };
                // std::allocator frees through sized delete. The debug CRT forwards it to unsized delete, but ASan's
                // replacement does not, so observe both overloads (a forwarded free finds no unfreed record twice).
                witest::detoured_thread_function<static_cast<void (*)(void*)>(&::operator delete)> release;
                REQUIRE_SUCCEEDED(release.reset([&](void* pointer) {
                    markFreed(pointer);
                    ::operator delete(pointer);
                }));
                witest::detoured_thread_function<static_cast<void (*)(void*, size_t)>(&::operator delete)> releaseSized;
                REQUIRE_SUCCEEDED(releaseSized.reset([&](void* pointer, size_t bytes) {
                    markFreed(pointer);
                    ::operator delete(pointer, bytes);
                }));
                const auto invoke = [&]() -> std::vector<std::wstring> {
                    if (route == 0 || route == 2)
                    {
                        return route == 0 ? wil::reg::get_value_multistring(HKEY_CURRENT_USER, threeParserValue) :
                            wil::reg::get_value<std::vector<std::wstring>>(HKEY_CURRENT_USER, threeParserValue);
                    }
                    auto result = route == 1 ? wil::reg::try_get_value_multistring(HKEY_CURRENT_USER, threeParserValue) :
                        wil::reg::try_get_value<std::vector<std::wstring>>(HKEY_CURRENT_USER, threeParserValue);
                    missing = !result;
                    return result ? std::move(*result) : std::vector<std::wstring>{};
                };
                const auto allocationSize = [&](const void* pointer) {
                    for (size_t index = recordCount; index > 0; --index)
                    {
                        const auto& record = records[index - 1];
                        if (record.pointer == pointer && record.afterRead && !record.freed)
                        {
                            return record.bytes;
                        }
                    }
                    return size_t{};
                };
                // Calibrate by returned storage identity, not the STL's unspecified allocation ordering or SSO.
                for (bool firstOnly : {true, false})
                {
                    recordCount = reads = successfulReads = 0;
                    source = firstOnly ? firstPayload.data() : payload.data();
                    sourceBytes = static_cast<DWORD>((firstOnly ? firstPayload.size() : payload.size()) * sizeof(wchar_t));
                    std::vector<std::wstring> strings;
                    active = true;
                    const auto hr = wil::ResultFromException([&] { strings = invoke(); });
                    active = false;
                    REQUIRE_SUCCEEDED(hr);
                    REQUIRE_FALSE(missing);
                    REQUIRE_FALSE(recordOverflow);
                    REQUIRE_FALSE(wrongFlags);
                    REQUIRE(reads == 2);
                    REQUIRE(successfulReads == 1);
                    REQUIRE(strings.size() == (firstOnly ? 1u : 3u));
                    for (size_t element = 0; element < strings.size(); ++element)
                    {
                        REQUIRE(strings[element].size() == lengths[element]);
                        REQUIRE(strings[element].data() != nullptr);
                        REQUIRE(strings[element].data()[lengths[element]] == L'\0');
                        REQUIRE(std::all_of(strings[element].begin(), strings[element].end(),
                            [element](wchar_t ch) { return ch == static_cast<wchar_t>(L'a' + element); }));
                    }
                    if (firstOnly)
                    {
                        allocationBytes[0] = allocationSize(strings.data());
                    }
                    else
                    {
                        for (size_t element = 0; element < strings.size(); ++element)
                        {
                            allocationBytes[element + 1] = allocationSize(strings[element].data());
                        }
                        size_t initialStorageAttempts = 0;
                        for (size_t index = 0; index < recordCount; ++index)
                        {
                            initialStorageAttempts += records[index].afterRead && records[index].bytes == allocationBytes[0];
                        }
                        REQUIRE(initialStorageAttempts == 1);
                    }
                }
                for (size_t index = 0; index < allocationBytes.size(); ++index)
                {
                    REQUIRE(allocationBytes[index] != 0);
                    for (size_t other = 0; other < index; ++other)
                    {
                        REQUIRE(allocationBytes[index] != allocationBytes[other]);
                    }
                }
                recordCount = reads = successfulReads = 0;
                source = stage == 4 ? emptyPayload : payload.data();
                sourceBytes = stage == 4 ? sizeof(emptyPayload) : static_cast<DWORD>(payload.size() * sizeof(wchar_t));
                std::vector<std::wstring> strings;
                inject = active = true;
                const auto hr = wil::ResultFromException([&] { strings = invoke(); });
                active = false;
                REQUIRE(hr == E_OUTOFMEMORY);
                REQUIRE_FALSE(missing);
                REQUIRE(strings.empty());
                REQUIRE_FALSE(recordOverflow);
                REQUIRE_FALSE(wrongFlags);
                REQUIRE(reads == 2);
                REQUIRE(successfulReads == 1);
                REQUIRE(failures == 1);
                REQUIRE(allocationsAfterFailure == 0);
                REQUIRE(stageAttempts[stage == 4 ? 0 : stage] == 1);
                for (size_t element = 1; element < stageAttempts.size(); ++element)
                {
                    REQUIRE(stageAttempts[element] == (stage < 4 && element <= stage ? 1u : 0u));
                }
                for (size_t index = 0; index < recordCount; ++index)
                {
                    REQUIRE(records[index].freed);
                }
            }
        }
    }
}
#endif

constexpr auto* dwordValueName = L"MyDwordValue";
constexpr auto* qwordValueName = L"MyQwordValue";
constexpr auto* stringValueName = L"MyStringValue";
#if defined(WIL_ENABLE_EXCEPTIONS)
constexpr auto* multiStringValueName = L"MyMultiStringValue";
#endif
constexpr auto* invalidValueName = L"NonExistentValue";
constexpr auto* wrongTypeValueName = L"InvalidTypeValue";

constexpr uint32_t test_dword_two = 2UL;
constexpr DWORD test_dword_three = 3UL;
constexpr uint32_t test_dword_zero = 0UL;
constexpr uint64_t test_qword_zero = 0ULL;
constexpr DWORD64 test_qword_max = 0xffffffffffffffff;
#ifdef WIL_ENABLE_EXCEPTIONS
const std::wstring test_string_empty{};
#endif

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

#ifdef WIL_ENABLE_EXCEPTIONS
// The empty multistring array has specific behavior: it will be read as an array with one string.
const std::vector<std::wstring> test_multistring_empty{};
#endif

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

#ifdef WIL_ENABLE_EXCEPTIONS
const std::vector<BYTE> emptyStringTestValue{};
#endif
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

static const wchar_t* enumTestNames[] = {
    L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    L"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    L"cccccccccccccccccccccc",
    L"dddddddddddddddddddddddddddddddd",
    L"eeeee",
    L""};

static wil::unique_cotaskmem_array_ptr<BYTE> cotaskmemArrayBytesTestArray[3];
static void PopulateCoTaskMemArrayTestCases()
{
    cotaskmemArrayBytesTestArray[0].reset(static_cast<BYTE*>(CoTaskMemAlloc(1)), 1);
    cotaskmemArrayBytesTestArray[0][0] = 0x00;

    cotaskmemArrayBytesTestArray[1].reset();

    cotaskmemArrayBytesTestArray[2].reset(static_cast<BYTE*>(CoTaskMemAlloc(15)), 15);
    constexpr BYTE thirdTestcaseData[]{0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    CopyMemory(cotaskmemArrayBytesTestArray[2].get(), thirdTestcaseData, 15);
}

// NOLINTNEXTLINE(misc-use-internal-linkage): Conditionally used & 'static' creates more warnings
bool AreStringsEqual(const std::wstring& lhs, const std::wstring& rhs) noexcept
{
    return lhs == rhs;
}

// Compares caller-owned storage against a counted UTF-16 expectation, including embedded NULs.
static bool AreStringsEqual(const wil::unique_bstr& lhs, const std::wstring& rhs) noexcept
{
    if (SysStringByteLen(lhs.get()) != rhs.length() * sizeof(WCHAR))
    {
        printf("String byte lengths don't match: BSTR %u, wstring %zu\n", SysStringByteLen(lhs.get()), rhs.length() * sizeof(WCHAR));
        return false;
    }
    if (!lhs && rhs.empty())
    {
        return true;
    }
    if (SysStringLen(lhs.get()) != rhs.length())
    {
        printf("String lengths don't match: BSTR (%ws) %u, wstring (%ws) %zu\n", lhs.get(), SysStringLen(lhs.get()), rhs.c_str(), rhs.length());
        return false;
    }
    return lhs && (lhs.get()[rhs.length()] == L'\0') && (0 == wmemcmp(lhs.get(), rhs.data(), rhs.length()));
}

#if defined(__WIL_OLEAUTO_H_STL)
// Compares caller-owned storage against a counted UTF-16 expectation, including embedded NULs.
static bool AreStringsEqual(const wil::shared_bstr& lhs, const std::wstring& rhs) noexcept
{
    if (SysStringByteLen(lhs.get()) != rhs.length() * sizeof(WCHAR))
    {
        printf("String byte lengths don't match: BSTR %u, wstring %zu\n", SysStringByteLen(lhs.get()), rhs.length() * sizeof(WCHAR));
        return false;
    }
    if (!lhs && rhs.empty())
    {
        return true;
    }
    if (SysStringLen(lhs.get()) != rhs.length())
    {
        printf("String lengths don't match: BSTR (%ws) %u, wstring (%ws) %zu\n", lhs.get(), SysStringLen(lhs.get()), rhs.c_str(), rhs.length());
        return false;
    }
    return lhs && (lhs.get()[rhs.length()] == L'\0') && (0 == wmemcmp(lhs.get(), rhs.data(), rhs.length()));
}
#endif

static bool AreStringsEqual(const wil::unique_cotaskmem_string& lhs, const std::wstring& rhs) noexcept
{
    if (!lhs && rhs.empty())
    {
        return true;
    }
    return (0 == wcscmp(lhs.get(), rhs.c_str()));
}

#if defined(__WIL_OBJBASE_H_STL)
static bool AreStringsEqual(const wil::shared_cotaskmem_string& lhs, const std::wstring& rhs) noexcept
{
    if (!lhs && rhs.empty())
    {
        return true;
    }
    return (0 == wcscmp(lhs.get(), rhs.c_str()));
}
#endif

template <size_t C>
static bool AreStringsEqual(wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string>& cotaskmemarray_strings, const PCWSTR array_literals[C])
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

static bool AreStringsEqual(wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string>& cotaskmem_array, const std::vector<std::wstring>& wstring_vector)
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

// NOLINTNEXTLINE(misc-use-internal-linkage): Conditionally used & 'static' creates more warnings
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
static void VerifyThrowsHr(HRESULT hr, const std::function<void()>& fn)
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

#if defined(WIL_ENABLE_EXCEPTIONS)
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::ExampleUsage
// Coverage: Exception-enabled only: exercises documentation examples for key open/create, DWORD/SZ get/set
// and optional try_get, including nothrow calls; lightly validates writes, child counts and registry error helpers.
// Also exercises last-write-time retrieval; these are usage examples rather than exhaustive assertions.
//----------------------------------------------------------------------------------------------------------------------
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
        (void)showTypeOverlay;
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
        std::ignore = wil::reg::get_value_dword(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme");
        const std::optional<std::wstring> stringOptional =
            wil::reg::try_get_value_string(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes", L"CurrentTheme");

        // Known HKEY
        const auto key =
            wil::reg::open_unique_key(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
        std::ignore = wil::reg::get_value_dword(key.get(), L"AppsUseLightTheme");

        // nothrow version, if you don't have exceptions
        wil::unique_bstr bstr;
        THROW_IF_FAILED(wil::reg::get_value_string_nothrow(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes", L"CurrentTheme", bstr));

        // Templated version
        std::ignore = wil::reg::get_value<std::wstring>(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes", L"CurrentTheme");

        // --- validation, not included in documentation ---
        const std::wstring expected{test_null_terminated_string};
        wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, stringValueName, expected.c_str());
        THROW_IF_FAILED(wil::reg::get_value_string_nothrow(HKEY_CURRENT_USER, testSubkey, stringValueName, bstr));
        REQUIRE(AreStringsEqual(bstr, expected));
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
        REQUIRE(wil::reg::get_child_value_count(key.get()) == 0);
        REQUIRE(wil::reg::get_child_key_count(key.get()) == 0);
        REQUIRE(wil::reg::get_child_key_count(HKEY_CLASSES_ROOT) > 1000);

        // Get last write time
        std::ignore = wil::reg::get_last_write_filetime(key.get());

        // Simple helpers for analyzing returned HRESULTs
        REQUIRE(wil::reg::is_registry_buffer_too_small(HRESULT_FROM_WIN32(ERROR_MORE_DATA)));
        REQUIRE(wil::reg::is_registry_not_found(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)));
        REQUIRE(wil::reg::is_registry_not_found(HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)));
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#pragma warning(default : 4189)
}
#endif // defined(WIL_ENABLE_EXCEPTIONS)

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::Open
// Coverage: Unique key open/create by handle or subkey path verifies DWORD/QWORD reads, read/write permissions,
// missing-key errors, access-flag mappings and child key/value counts through nothrow APIs.
// Shared HKEY cases use WINREG_STL guards; throwing open/create/get and count cases require exceptions.
//----------------------------------------------------------------------------------------------------------------------
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
    SECTION("key_access::write with opened key")
    {
        constexpr auto* subSubKey = L"subkey";

        // create the key hierarchy with full access
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(hkey.get(), subSubKey, subkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(subkey.get(), dwordValueName, test_dword_two));
        subkey.reset();

        // open with write access
        wil::unique_hkey write_key;
        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(hkey.get(), subSubKey, write_key, wil::reg::key_access::write));

        // write should succeed
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(write_key.get(), dwordValueName, test_dword_three));

        // read should fail with access denied — proves this is KEY_WRITE, not KEY_ALL_ACCESS
        DWORD result{};
        auto hr = wil::reg::get_value_dword_nothrow(write_key.get(), dwordValueName, &result);
        REQUIRE(hr == E_ACCESSDENIED);
    }
    SECTION("key_access::write with string key")
    {
        // create the key with full access and write a test value
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, test_dword_two));
        hkey.reset();

        // open with write access
        wil::unique_hkey write_key;
        REQUIRE_SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_key, wil::reg::key_access::write));

        // write should succeed
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(write_key.get(), dwordValueName, test_dword_three));

        // read should fail with access denied
        DWORD result{};
        auto hr = wil::reg::get_value_dword_nothrow(write_key.get(), dwordValueName, &result);
        REQUIRE(hr == E_ACCESSDENIED);
    }
    SECTION("key_access::write with create_unique_key_nothrow")
    {
        // create with write access
        wil::unique_hkey write_key;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, write_key, wil::reg::key_access::write));

        // write should succeed
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(write_key.get(), dwordValueName, test_dword_two));

        // read should fail with access denied
        DWORD result{};
        auto hr = wil::reg::get_value_dword_nothrow(write_key.get(), dwordValueName, &result);
        REQUIRE(hr == E_ACCESSDENIED);
    }
    SECTION("get_access_flags maps key_access values correctly")
    {
        // verify the constexpr flag mapping is correct for all key_access values
        static_assert(wil::reg::reg_view_details::get_access_flags(wil::reg::key_access::read) == KEY_READ, "read must map to KEY_READ");
        static_assert(wil::reg::reg_view_details::get_access_flags(wil::reg::key_access::write) == KEY_WRITE, "write must map to KEY_WRITE");
        static_assert(
            wil::reg::reg_view_details::get_access_flags(wil::reg::key_access::readwrite) == KEY_ALL_ACCESS,
            "readwrite must map to KEY_ALL_ACCESS");
        static_assert(
            wil::reg::reg_view_details::get_access_flags(wil::reg::key_access::read64) == (KEY_READ | KEY_WOW64_64KEY),
            "read64 must map to KEY_READ | KEY_WOW64_64KEY");
        static_assert(
            wil::reg::reg_view_details::get_access_flags(wil::reg::key_access::write64) == (KEY_WRITE | KEY_WOW64_64KEY),
            "write64 must map to KEY_WRITE | KEY_WOW64_64KEY");
        static_assert(
            wil::reg::reg_view_details::get_access_flags(wil::reg::key_access::readwrite64) == (KEY_ALL_ACCESS | KEY_WOW64_64KEY),
            "readwrite64 must map to KEY_ALL_ACCESS | KEY_WOW64_64KEY");
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

        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, 1UL));
        REQUIRE_SUCCEEDED(wil::reg::set_value_qword_nothrow(hkey.get(), qwordValueName, 2ULL));
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

        wil::reg::set_value_dword(hkey.get(), dwordValueName, 1UL);
        wil::reg::set_value_qword(hkey.get(), qwordValueName, 2ULL);
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

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::delete_value_and_tree
// Coverage: Nothrow value deletion by handle/path removes DWORD values and reports missing values;
// tree deletion removes descendants and tolerates missing trees.
// Exception-enabled delete_value/delete_tree and optional DWORD reads verify the corresponding removal behavior.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::delete_value_and_tree", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("delete_value_nothrow: from an open key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, test_dword_two));

        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(hkey.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);

        REQUIRE_SUCCEEDED(wil::reg::delete_value_nothrow(hkey.get(), dwordValueName));
        // the value should now be gone
        REQUIRE(wil::reg::get_value_dword_nothrow(hkey.get(), dwordValueName, &result) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        // deleting a value that doesn't exist returns not-found
        REQUIRE(wil::reg::delete_value_nothrow(hkey.get(), dwordValueName) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    }

    SECTION("delete_value_nothrow: via subkey path")
    {
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_two));

        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result));
        REQUIRE(result == test_dword_two);

        REQUIRE_SUCCEEDED(wil::reg::delete_value_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName));
        REQUIRE(
            wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, testSubkey, dwordValueName, &result) ==
            HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    }

    SECTION("delete_tree_nothrow: removes subkeys and values")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        wil::unique_hkey subkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(hkey.get(), L"child", subkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(subkey.get(), dwordValueName, test_dword_two));

        REQUIRE_SUCCEEDED(wil::reg::delete_tree_nothrow(HKEY_CURRENT_USER, testSubkey));
        wil::unique_hkey opened;
        REQUIRE(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

        // deleting a tree that doesn't exist succeeds (is_registry_not_found is swallowed)
        REQUIRE_SUCCEEDED(wil::reg::delete_tree_nothrow(HKEY_CURRENT_USER, testSubkey));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("delete_value/delete_tree: throwing variants")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value_dword(hkey.get(), dwordValueName, test_dword_two);
        wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, stringValueName, test_dword_three);

        wil::reg::delete_value(hkey.get(), dwordValueName);
        REQUIRE(!wil::reg::try_get_value_dword(hkey.get(), dwordValueName).has_value());

        wil::reg::delete_value(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(!wil::reg::try_get_value_dword(HKEY_CURRENT_USER, testSubkey, stringValueName).has_value());

        wil::reg::delete_tree(HKEY_CURRENT_USER, testSubkey);
        wil::unique_hkey opened;
        REQUIRE(wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, opened) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        // deleting a non-existent tree does not throw
        wil::reg::delete_tree(HKEY_CURRENT_USER, testSubkey);
    }
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::simple types typed nothrow gets/sets
// Coverage: Named and generic DWORD/QWORD nothrow get/set cases by open HKEY or subkey path;
// round-trip zero/one/max values, reused outputs and default names, plus missing-value and wrong-type errors.
//----------------------------------------------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::simple types typed gets/sets/try_gets
// Coverage: Exception-enabled only: named/generic DWORD, QWORD and vector<wstring> MULTI get/set cases
// round-trip values and default names by HKEY/path, and reject missing values or wrong stored types.
// Optional try_get cases return values/nullopt for present/missing data but still throw for type mismatches.
//----------------------------------------------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::wstrings
// Coverage: Exception-enabled only: REG_SZ wstring generic nothrow and named get reads handle zero-byte
// and unterminated values by HKEY/path; fixed WCHAR named nothrow reads verify default names, required byte sizes,
// small-buffer errors, missing values and rejection of stored DWORD data.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::wstrings", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

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
    const std::function<HRESULT(PCWSTR, typename type_identity<StringT>::type&)>& getFn,
    const std::function<HRESULT(PCWSTR, typename type_identity<SetStringT>::type)>& setFn,
    const std::function<HRESULT(PCWSTR)>& wrongSetFn)
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
    const std::function<typename type_identity<StringT>::type(PCWSTR)>& getFn,
    const std::function<void(PCWSTR, typename type_identity<StringSetT>::type)>& setFn,
    const std::function<void(PCWSTR)>& setWrongTypeFn)
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
        [&](PCWSTR valueName, PCWSTR dataValue) -> void {
            wil::reg::set_value_string(hkey.get(), valueName, dataValue);
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
        [](PCWSTR valueName, PCWSTR dataValue) -> void {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, valueName, dataValue);
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
        [&](PCWSTR valueName, PCWSTR dataValue) -> void {
            wil::reg::set_value(hkey.get(), valueName, dataValue);
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
        [](PCWSTR valueName, PCWSTR dataValue) -> void {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, valueName, dataValue);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}

#if defined(__cpp_lib_optional)
template <typename StringT, typename StringSetT = PCWSTR>
void verify_try_string(
    const std::function<std::optional<StringT>(PCWSTR)>& tryGetFn,
    const std::function<void(PCWSTR, typename type_identity<StringSetT>::type)>& setFn,
    const std::function<void(PCWSTR)>& setWrongTypeFn)
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
        [&](PCWSTR valueName, PCWSTR dataValue) -> void {
            wil::reg::set_value_string(hkey.get(), valueName, dataValue);
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
        [](PCWSTR valueName, PCWSTR dataValue) -> void {
            wil::reg::set_value_string(HKEY_CURRENT_USER, testSubkey, valueName, dataValue);
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
        [&](PCWSTR valueName, PCWSTR dataValue) -> void {
            wil::reg::set_value(hkey.get(), valueName, dataValue);
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
        [](PCWSTR valueName, PCWSTR dataValue) -> void {
            wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, valueName, dataValue);
        },
        [](PCWSTR valueName) {
            wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, valueName, test_dword_zero);
        });
}
#endif // defined(__cpp_lib_optional)
#endif
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::string types
// Coverage: REG_SZ named/generic get/set and nothrow calls by HKEY/path verify empty/normal/default values,
// output reuse, missing values and DWORD mismatches with fixed WCHAR and OLEAUTO/OBJBASE BSTR/CoTaskMem owners.
// Shared owners, wstring get/set lvalues and get/optional try_get cases require exceptions and their STL guards.
//----------------------------------------------------------------------------------------------------------------------
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

    SECTION("strings set_value with std::wstring lvalue: with opened key")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));

        // verify non-const std::wstring lvalue works with set_value (issue #624)
        std::wstring stringValue{L"wstring lvalue test"};
        wil::reg::set_value(hkey.get(), stringValueName, stringValue);
        auto result = wil::reg::get_value<std::wstring>(hkey.get(), stringValueName);
        REQUIRE(result == stringValue);

        // verify const std::wstring lvalue also works
        const std::wstring constStringValue{L"const wstring test"};
        wil::reg::set_value(hkey.get(), stringValueName, constStringValue);
        result = wil::reg::get_value<std::wstring>(hkey.get(), stringValueName);
        REQUIRE(result == constStringValue);

        // verify empty std::wstring
        std::wstring emptyValue;
        wil::reg::set_value(hkey.get(), stringValueName, emptyValue);
        result = wil::reg::get_value<std::wstring>(hkey.get(), stringValueName);
        REQUIRE(result.empty());
    }

    SECTION("strings set_value with std::wstring lvalue: with string key")
    {
        std::wstring stringValue{L"wstring lvalue subkey test"};
        wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, stringValue);
        auto result = wil::reg::get_value<std::wstring>(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == stringValue);

        const std::wstring constStringValue{L"const wstring subkey test"};
        wil::reg::set_value(HKEY_CURRENT_USER, testSubkey, stringValueName, constStringValue);
        result = wil::reg::get_value<std::wstring>(HKEY_CURRENT_USER, testSubkey, stringValueName);
        REQUIRE(result == constStringValue);
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

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::expanded_wstring
// Coverage: REG_EXPAND_SZ named nothrow get/set by HKEY/path uses fixed WCHAR arrays, compared with
// ExpandEnvironmentStringsW for empty/literal/environment values and default names; checks required byte sizes,
// small-buffer errors, missing values and DWORD type rejection.
//----------------------------------------------------------------------------------------------------------------------
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
    const std::function<HRESULT(PCWSTR, typename type_identity<StringT>::type&)>& getFn,
    const std::function<HRESULT(PCWSTR, typename type_identity<SetStringT>::type)>& setFn,
    const std::function<HRESULT(PCWSTR)>& setWrongTypeFn)
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
    const std::function<typename type_identity<StringT>::type(PCWSTR)>& getFn,
    const std::function<void(PCWSTR, typename type_identity<SetStringT>::type)>& setFn,
    const std::function<void(PCWSTR)>& setWrongTypeFn)
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
    const std::function<std::optional<typename type_identity<StringT>::type>(PCWSTR)>& getFn,
    const std::function<void(PCWSTR, typename type_identity<SetStringT>::type)>& setFn,
    const std::function<void(PCWSTR)>& setWrongTypeFn)
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

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::expanded_string
// Coverage: REG_EXPAND_SZ named get/set by HKEY/path compares owning strings with ExpandEnvironmentStringsW;
// nothrow cases verify output reuse, defaults, missing values and DWORD mismatches.
// OLEAUTO guards select BSTR/CoTaskMem cases; shared owners and wstring get/optional try_get require exceptions.
//----------------------------------------------------------------------------------------------------------------------
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

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::multi-strings
// Coverage: REG_MULTI_SZ named/generic readers handle empty, odd-byte, unterminated and embedded-null layouts;
// OBJBASE CoTaskMem nothrow cases also check output reuse and default names by HKEY/path.
// Exception-enabled vector get and optional try_get cases verify parsed elements and empty-array write semantics.
//----------------------------------------------------------------------------------------------------------------------
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
static void verify_cotaskmem_array_nothrow(
    const std::function<HRESULT(PCWSTR, DWORD, wil::unique_cotaskmem_array_ptr<BYTE>&)>& getFn,
    const std::function<HRESULT(PCWSTR, DWORD, const wil::unique_cotaskmem_array_ptr<BYTE>&)>& setFn,
    const std::function<HRESULT(PCWSTR, uint32_t)>& setDwordFn)
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

#ifdef WIL_ENABLE_EXCEPTIONS
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
    const std::function<HRESULT(PCWSTR, DWORD, wil::unique_cotaskmem_array_ptr<BYTE>&)>& getFn,
    const std::function<void(PCWSTR, DWORD, const std::vector<BYTE>&)>& setFn,
    const std::function<HRESULT(PCWSTR, uint32_t)>& setDwordFn)
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
    const std::function<std::vector<BYTE>(PCWSTR, DWORD)>& getFn,
    const std::function<void(PCWSTR, DWORD, const std::vector<BYTE>&)>& setFn,
    const std::function<void(PCWSTR, uint32_t)>& setDwordFn)
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
    const std::function<std::optional<std::vector<BYTE>>(PCWSTR, DWORD)>& tryGetFn,
    const std::function<void(PCWSTR, DWORD, const std::vector<BYTE>&)>& setFn,
    const std::function<void(PCWSTR, uint32_t)>& setDwordFn)
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

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::vector-bytes
// Coverage: Exception-enabled only: binary named get/optional try_get into vector<BYTE> and nothrow CoTaskMem
// reads by HKEY/path round-trip REG_BINARY buffers, defaults and reused outputs; reject missing/wrong types
// and read stored DWORD bytes when explicitly requested as REG_DWORD.
//----------------------------------------------------------------------------------------------------------------------
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
#endif

#if defined(__WIL_OBJBASE_H_)
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::cotaskmem_array-bytes
// Coverage: OBJBASE-gated binary named nothrow get/set by HKEY/path round-trips CoTaskMem byte arrays,
// empty/default values and reused outputs; checks missing/type errors and explicitly selected DWORD bytes.
//----------------------------------------------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::value_iterator
// Coverage: Exception-enabled STL value_iterator enumerates wstring names and DWORD/QWORD/SZ type metadata;
// checks empty/single/many values, copy/assignment, dereference/increment, name resizing and maximum lengths
// through algorithms and range-for loops, without parsing value payloads.
//----------------------------------------------------------------------------------------------------------------------
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto nameAndType) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
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
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2) - 2;
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
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1UL);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ULL);
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
                FAIL();
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
                FAIL();
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
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1UL);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ULL);
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
                FAIL();
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
                FAIL();
            }
        }
        REQUIRE(count == 4);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::key_iterator
// Coverage: Exception-enabled STL key_iterator enumerates subkey names for empty/single/many keys;
// verifies copy/assignment, dereference/increment, resizing and maximum names via algorithms and range-for loops.
// Counting also uses WINREG_STL-gated shared HKEYs; empty subkey names do not create another child.
//----------------------------------------------------------------------------------------------------------------------
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto keyInfo) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
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
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2) - 1;
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
                FAIL();
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
                FAIL();
            }
        }
    }
}

#if defined(__WIL_OLEAUTO_H_)
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::value_bstr_iterator
// Coverage: Exception-enabled OLEAUTO value_bstr_iterator enumerates BSTR names and DWORD/QWORD/SZ types;
// checks empty/single/many values, copy/assignment, dereference/increment, BSTR length and maximum-name resizing
// through algorithms and range-for loops, without parsing value payloads.
//----------------------------------------------------------------------------------------------------------------------
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
        REQUIRE(AreStringsEqual((*test_iterator).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator->name, test_enum_valueName1));
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*test_iterator_copy).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator_copy->name, test_enum_valueName1));
        ++test_iterator;
        REQUIRE(test_iterator == wil::reg::value_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_iterator{});

        test_iterator = wil::reg::value_bstr_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::value_bstr_iterator{});
        REQUIRE(test_iterator == wil::reg::value_bstr_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*test_iterator).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator->name, test_enum_valueName1));
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*test_iterator_copy).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator_copy->name, test_enum_valueName1));
        ++test_iterator;
        REQUIRE(test_iterator == wil::reg::value_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_iterator{});

        const wil::unique_hkey hkey2{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        test_iterator = wil::reg::value_bstr_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::value_bstr_iterator{});
        REQUIRE(AreStringsEqual((*test_iterator).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator->name, test_enum_valueName1));
        test_iterator_copy = test_iterator;
        REQUIRE(AreStringsEqual((*test_iterator_copy).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator_copy->name, test_enum_valueName1));
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto nameAndType) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(nameAndType.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](const auto& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(nameAndType.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](auto&& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(nameAndType.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames));
    }

    SECTION("value_bstr_iterator max name lengths")
    {
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2) - 2;
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
        REQUIRE(AreStringsEqual(half_length_value.name, half_length_name.get()));
        ++iterate_values;
        const auto& full_length_value = *iterate_values;
        REQUIRE(full_length == SysStringLen(full_length_value.name.get()));
        REQUIRE(AreStringsEqual(full_length_value.name, full_length_name.get()));
        ++iterate_values;
        REQUIRE(iterate_values == wil::reg::value_bstr_iterator{});
    }

    SECTION("value_bstr_iterator with many values - std::for_each usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(hkey.get(), test_enum_valueName1, 0);
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1UL);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ULL);
        wil::reg::set_value(hkey.get(), test_enum_valueName4, L"four");

        uint32_t count = 0;
        std::for_each(wil::reg::value_bstr_iterator(hkey.get()), wil::reg::value_bstr_iterator{}, [&](const auto& value_data) {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                FAIL();
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
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                FAIL();
            }
        });
        REQUIRE(count == 4);

        auto std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (AreStringsEqual(value_data.name, test_enum_valueName1)) && (value_data.type == REG_DWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (AreStringsEqual(value_data.name, test_enum_valueName2)) && (value_data.type == REG_DWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (AreStringsEqual(value_data.name, test_enum_valueName3)) && (value_data.type == REG_QWORD);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (AreStringsEqual(value_data.name, test_enum_valueName4)) && (value_data.type == REG_SZ);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::value_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::value_bstr_iterator{},
            [&](const auto& value_data) {
                return (AreStringsEqual(value_data.name, L"xyz"));
            });
        REQUIRE(std_count == 0);
    }

    SECTION("value_bstr_iterator with many values - range-for iterator usage")
    {
        wil::unique_hkey hkey{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access::readwrite)};
        wil::reg::set_value(hkey.get(), test_enum_valueName1, 0);
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1UL);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ULL);
        wil::reg::set_value(hkey.get(), test_enum_valueName4, L"four");

        uint32_t count = 0;
        for (const auto& value_data : wil::make_range(wil::reg::value_bstr_iterator{hkey.get()}, wil::reg::value_bstr_iterator{}))
        {
            ++count;
            switch (count)
            {
            case 1:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                FAIL();
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
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                FAIL();
            }
        }
        REQUIRE(count == 4);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::key_bstr_iterator
// Coverage: Exception-enabled OLEAUTO key_bstr_iterator enumerates BSTR subkey names for empty/single/many keys;
// checks copy/assignment, dereference/increment, lengths, resizing and maximum names via algorithms/range-for.
// Counting also uses WINREG_STL-gated shared HKEYs; empty subkey names do not create another child.
//----------------------------------------------------------------------------------------------------------------------
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
        REQUIRE(AreStringsEqual((*test_iterator).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(test_iterator->name, test_enum_KeyName1));
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_bstr_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*test_iterator_copy).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(test_iterator_copy->name, test_enum_KeyName1));
        ++test_iterator;
        REQUIRE(test_iterator == key_end);
        REQUIRE(test_iterator == wil::reg::key_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == key_end);
        REQUIRE(test_iterator_copy == wil::reg::key_bstr_iterator{});

        test_iterator = wil::reg::key_bstr_iterator(hkey.get());
        REQUIRE(test_iterator != wil::reg::key_bstr_iterator{});
        REQUIRE(test_iterator == wil::reg::key_bstr_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*test_iterator).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(test_iterator->name, test_enum_KeyName1));
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::key_bstr_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*test_iterator_copy).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(test_iterator_copy->name, test_enum_KeyName1));
        ++test_iterator;
        REQUIRE(test_iterator == key_end);
        REQUIRE(test_iterator == wil::reg::key_bstr_iterator{});
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == key_end);
        REQUIRE(test_iterator_copy == wil::reg::key_bstr_iterator{});

        const wil::unique_hkey hkey2{wil::reg::create_unique_key(HKEY_CURRENT_USER, testSubkey)};
        test_iterator = wil::reg::key_bstr_iterator(hkey2.get());
        REQUIRE(test_iterator != wil::reg::key_bstr_iterator{});
        REQUIRE(AreStringsEqual((*test_iterator).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(test_iterator->name, test_enum_KeyName1));
        test_iterator_copy = test_iterator;
        REQUIRE(AreStringsEqual((*test_iterator_copy).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(test_iterator_copy->name, test_enum_KeyName1));
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto keyInfo) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(keyInfo.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](const auto& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(keyInfo.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](auto&& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(keyInfo.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty
    }

    SECTION("key_bstr_iterator max name lengths")
    {
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2) - 1;
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
        REQUIRE(AreStringsEqual(half_length_key.name, half_length_name.get()));
        ++iterate_keys;
        const auto& full_length_key = *iterate_keys;
        REQUIRE(full_length == SysStringLen(full_length_key.name.get()));
        REQUIRE(AreStringsEqual(full_length_key.name, full_length_name.get()));
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
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName4));
                break;
            default:
                FAIL();
            }
        });
        REQUIRE(count == 4);

        auto std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, test_enum_KeyName1);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, test_enum_KeyName2);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, test_enum_KeyName3);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, test_enum_KeyName4);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, L"xyz");
            });
        REQUIRE(std_count == 0);

#if defined(__WIL_WINREG_STL)
        // repeat with wil::shared_hkey
        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, test_enum_KeyName1);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, test_enum_KeyName2);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, test_enum_KeyName3);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, test_enum_KeyName4);
            });
        REQUIRE(std_count == 1);

        std_count = std::count_if(
            wil::reg::key_bstr_iterator(wil::reg::open_shared_key(HKEY_CURRENT_USER, testSubkey).get()),
            wil::reg::key_bstr_iterator{},
            [](const auto& test_data) {
                return AreStringsEqual(test_data.name, L"xyz");
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
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName4));
                break;
            default:
                FAIL();
            }
        }
    }
}
#endif

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::value_heap_string_iterator
// Coverage: Exception-enabled only: value_heap_string_iterator enumerates process-heap names and DWORD/QWORD/SZ
// type metadata; checks empty/single/many values, copies, assignment, dereference/increment and maximum-name
// resizing through algorithms/range-for loops, without parsing value payloads.
//----------------------------------------------------------------------------------------------------------------------
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto nameAndType) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
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
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2) - 2;
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
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1UL);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ULL);
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
                FAIL();
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
                FAIL();
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
        wil::reg::set_value(hkey.get(), test_enum_valueName2, 1UL);
        wil::reg::set_value(hkey.get(), test_enum_valueName3, 3ULL);
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
                FAIL();
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
                FAIL();
            }
        }
        REQUIRE(count == 4);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::key_heap_string_iterator
// Coverage: Exception-enabled only: key_heap_string_iterator enumerates process-heap subkey names;
// checks empty/single/many keys, copies, assignment, dereference/increment, resizing and maximum-name lengths
// via algorithms/range-for; counting also uses WINREG_STL-gated shared HKEYs.
//----------------------------------------------------------------------------------------------------------------------
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto keyInfo) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
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
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2) - 2;
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
                FAIL();
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
                FAIL();
            }
        }
    }
}
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_OLEAUTO_H_)
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::value_bstr_nothrow_iterator
// Coverage: OLEAUTO-gated nothrow value enumeration checks BSTR names and DWORD/QWORD/SZ type metadata;
// verifies copies, assignment, dereference, maximum-name resizing, move_next/last_error and manual/range loops.
// Empty keys and invalid handles compare at end; no value payload parsing is exercised.
//----------------------------------------------------------------------------------------------------------------------
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
        REQUIRE(AreStringsEqual((*test_iterator).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator->name, test_enum_valueName1));
        auto test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*test_iterator_copy).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator_copy->name, test_enum_valueName1));
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
        REQUIRE(AreStringsEqual((*test_iterator).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator->name, test_enum_valueName1));
        REQUIRE(!test_iterator.at_end());
        test_iterator_copy = test_iterator;
        REQUIRE(test_iterator_copy == wil::reg::value_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*test_iterator_copy).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator_copy->name, test_enum_valueName1));
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
        REQUIRE(AreStringsEqual((*test_iterator).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator->name, test_enum_valueName1));
        REQUIRE(!test_iterator.at_end());
        test_iterator_copy = test_iterator;
        REQUIRE(AreStringsEqual((*test_iterator_copy).name, test_enum_valueName1));
        REQUIRE(AreStringsEqual(test_iterator_copy->name, test_enum_valueName1));
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto nameAndType) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(nameAndType.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](const auto& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(nameAndType.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames));

        count = 0;
        std::for_each(begin, end, [&](auto&& nameAndType) {
            auto stringLength = wcslen(nameAndType.name.get());
            REQUIRE(stringLength == SysStringLen(nameAndType.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(nameAndType.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames));
    }

    SECTION("value_bstr_nothrow_iterator max name lengths")
    {
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2) - 2;
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
        REQUIRE(AreStringsEqual(half_length_value.name, half_length_name.get()));
        ++iterate_values;
        REQUIRE_SUCCEEDED(iterate_values.last_error());
        const auto& full_length_value = *iterate_values;
        REQUIRE(full_length == SysStringLen(full_length_value.name.get()));
        REQUIRE(AreStringsEqual(full_length_value.name, full_length_name.get()));
        ++iterate_values;
        REQUIRE(iterate_values == wil::reg::value_bstr_nothrow_iterator{});
    }

    SECTION("value_bstr_nothrow_iterator with many values - range-for iterator usage")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName1, 0));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName2, 1UL));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName3, 3ULL));
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
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                FAIL();
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
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName1));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 2:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName2));
                REQUIRE(value_data.type == REG_DWORD);
                break;
            case 3:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName3));
                REQUIRE(value_data.type == REG_QWORD);
                break;
            case 4:
                REQUIRE(AreStringsEqual(value_data.name, test_enum_valueName4));
                REQUIRE(value_data.type == REG_SZ);
                break;
            default:
                FAIL();
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
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_valueName1));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 2:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_valueName2));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 3:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_valueName3));
                REQUIRE(manual_iterator->type == REG_QWORD);
                break;
            case 4:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_valueName4));
                REQUIRE(manual_iterator->type == REG_SZ);
                break;
            default:
                FAIL();
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
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_valueName1));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 2:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_valueName2));
                REQUIRE(manual_iterator->type == REG_DWORD);
                break;
            case 3:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_valueName3));
                REQUIRE(manual_iterator->type == REG_QWORD);
                break;
            case 4:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_valueName4));
                REQUIRE(manual_iterator->type == REG_SZ);
                break;
            default:
                FAIL();
            }
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(manual_iterator->at_end());
        REQUIRE(count == 4);
    }

    SECTION("value_bstr_nothrow_iterator with invalid handle")
    {
        auto test_iterator = wil::reg::value_bstr_nothrow_iterator{(HKEY)INVALID_HANDLE_VALUE};
        auto test_end_iterator = wil::reg::value_bstr_nothrow_iterator{};
        REQUIRE(test_iterator == test_end_iterator);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::key_bstr_nothrow_iterator
// Coverage: OLEAUTO-gated nothrow subkey enumeration checks BSTR name lengths and maximum-name resizing;
// verifies copies, assignment, dereference, move_next/last_error and algorithm/manual/range iteration.
// Empty keys and invalid handles compare at end; empty subkey names do not create another child.
//----------------------------------------------------------------------------------------------------------------------
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
        REQUIRE(AreStringsEqual((*key_iterator).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(key_iterator->name, test_enum_KeyName1));
        auto key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == wil::reg::key_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*key_iterator_copy).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(key_iterator_copy->name, test_enum_KeyName1));
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
        REQUIRE(AreStringsEqual((*key_iterator).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(key_iterator->name, test_enum_KeyName1));
        key_iterator_copy = key_iterator;
        REQUIRE(key_iterator_copy == wil::reg::key_bstr_nothrow_iterator(hkey.get()));
        REQUIRE(AreStringsEqual((*key_iterator_copy).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(key_iterator_copy->name, test_enum_KeyName1));
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
        REQUIRE(AreStringsEqual((*key_iterator).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(key_iterator->name, test_enum_KeyName1));
        key_iterator_copy = key_iterator;
        REQUIRE(AreStringsEqual((*key_iterator_copy).name, test_enum_KeyName1));
        REQUIRE(AreStringsEqual(key_iterator_copy->name, test_enum_KeyName1));
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto keyInfo) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(keyInfo.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](const auto& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(keyInfo.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty

        count = 0;
        std::for_each(begin, end, [&](auto&& keyInfo) {
            auto stringLength = wcslen(keyInfo.name.get());
            REQUIRE(stringLength == ::SysStringLen(keyInfo.name.get()));
            REQUIRE(stringLength == wcslen(enumTestNames[count]));
            REQUIRE(AreStringsEqual(keyInfo.name, enumTestNames[count]));
            ++count;
        });
        REQUIRE_SUCCEEDED(begin.last_error());
        REQUIRE(count == std::size(enumTestNames) - 1); // cannot create the last key as its name is empty
    }

    SECTION("key_bstr_nothrow_iterator max name lengths")
    {
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2) - 1;
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
        REQUIRE(AreStringsEqual(half_length_key.name, half_length_name.get()));
        ++iterate_keys;
        REQUIRE_SUCCEEDED(iterate_keys.last_error());
        const auto& full_length_key = *iterate_keys;
        REQUIRE(full_length == SysStringLen(full_length_key.name.get()));
        REQUIRE(AreStringsEqual(full_length_key.name, full_length_name.get()));
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
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName4));
                break;
            default:
                FAIL();
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
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(AreStringsEqual(key_data.name, test_enum_KeyName4));
                break;
            default:
                FAIL();
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
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_KeyName4));
                break;
            default:
                FAIL();
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
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_KeyName1));
                break;
            case 2:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_KeyName2));
                break;
            case 3:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_KeyName3));
                break;
            case 4:
                REQUIRE(AreStringsEqual(manual_iterator->name, test_enum_KeyName4));
                break;
            default:
                FAIL();
            }
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(count == 4);
    }

    SECTION("key_bstr_nothrow_iterator with invalid handle")
    {
        auto test_iterator = wil::reg::key_bstr_nothrow_iterator{(HKEY)INVALID_HANDLE_VALUE};
        auto test_end_iterator = wil::reg::key_bstr_nothrow_iterator{};
        REQUIRE(test_iterator == test_end_iterator);
    }
}
#endif
//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::value_heap_string_nothrow_iterator
// Coverage: Nothrow value enumeration checks process-heap names and DWORD/QWORD/SZ type metadata;
// verifies copies, assignment, dereference, maximum-name resizing, move_next/last_error and manual/range loops.
// Empty keys and invalid handles compare at end; no value payload parsing is exercised.
//----------------------------------------------------------------------------------------------------------------------
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto nameAndType) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
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
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_valuename_length / 2) - 2;
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
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName2, 1UL));
        REQUIRE_SUCCEEDED(wil::reg::set_value_nothrow(hkey.get(), test_enum_valueName3, 3ULL));
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
                FAIL();
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
                FAIL();
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
                FAIL();
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
                FAIL();
            }
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(manual_iterator->at_end());
        REQUIRE(count == 4);
    }

    SECTION("value_heap_string_nothrow_iterator with invalid handle")
    {
        auto test_iterator = wil::reg::value_heap_string_nothrow_iterator{(HKEY)INVALID_HANDLE_VALUE};
        auto test_end_iterator = wil::reg::value_heap_string_nothrow_iterator{};
        REQUIRE(test_iterator == test_end_iterator);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::key_heap_string_nothrow_iterator
// Coverage: Nothrow subkey enumeration checks process-heap name lengths and maximum-name resizing;
// verifies copies, assignment, dereference, move_next/last_error and algorithm/manual/range iteration.
// Empty keys and invalid handles compare at end; empty subkey names do not create another child.
//----------------------------------------------------------------------------------------------------------------------
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

        size_t count = 0U;
        std::for_each(begin, end, [&](auto keyInfo) { // NOLINT(performance-unnecessary-value-param): Copy is intentional for testing
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
        const auto half_length = (::wil::reg::reg_iterator_details::iterator_max_keyname_length / 2) - 2;
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
                FAIL();
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
                FAIL();
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
                FAIL();
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
                FAIL();
            }
        }
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE_SUCCEEDED(manual_iterator.last_error());
        REQUIRE(count == 4);
    }

    SECTION("key_heap_string_nothrow_iterator with invalid handle")
    {
        auto test_iterator = wil::reg::key_heap_string_nothrow_iterator{(HKEY)INVALID_HANDLE_VALUE};
        auto test_end_iterator = wil::reg::key_heap_string_nothrow_iterator{};
        REQUIRE(test_iterator == test_end_iterator);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::raw_regsam_access
// Coverage: Access-mask mappings and bitwise composition support raw REGSAM key open/create;
// nothrow DWORD reads succeed with QUERY_VALUE while writes are denied, and named access values still work.
// Exception-enabled raw-mask open/get verifies the same read permission.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::raw_regsam_access", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    // key_access values map directly to their REGSAM masks, and arbitrary masks can be composed with the flag operators.
    static_assert(static_cast<DWORD>(wil::reg::key_access::read) == KEY_READ, "read must map to KEY_READ");
    static_assert(static_cast<DWORD>(wil::reg::key_access::write) == KEY_WRITE, "write must map to KEY_WRITE");
    static_assert(static_cast<DWORD>(wil::reg::key_access::readwrite) == KEY_ALL_ACCESS, "readwrite must map to KEY_ALL_ACCESS");
    static_assert(static_cast<DWORD>(wil::reg::key_access::read64) == (KEY_READ | KEY_WOW64_64KEY), "read64 must map to KEY_READ | KEY_WOW64_64KEY");
    static_assert(
        static_cast<DWORD>(wil::reg::key_access::read | wil::reg::key_access::write) == (KEY_READ | KEY_WRITE),
        "key_access values must combine with the bitwise OR operator");

    SECTION("create/open with a raw REGSAM mask")
    {
        // create with an explicit mask that has no named key_access value
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(
            wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access{KEY_QUERY_VALUE | KEY_SET_VALUE}));
        REQUIRE_SUCCEEDED(wil::reg::set_value_dword_nothrow(hkey.get(), dwordValueName, test_dword_two));

        // open with KEY_QUERY_VALUE only - reading works, writing is denied
        wil::unique_hkey readonly;
        REQUIRE_SUCCEEDED(
            wil::reg::open_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, readonly, wil::reg::key_access{KEY_QUERY_VALUE}));
        DWORD result{};
        REQUIRE_SUCCEEDED(wil::reg::get_value_dword_nothrow(readonly.get(), dwordValueName, &result));
        REQUIRE(result == test_dword_two);
        REQUIRE(wil::reg::set_value_dword_nothrow(readonly.get(), dwordValueName, test_dword_three) == E_ACCESSDENIED);
    }

    SECTION("named key_access values still work")
    {
        wil::unique_hkey hkey;
        REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
        REQUIRE(hkey.get() != nullptr);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("throwing open with a raw REGSAM mask")
    {
        wil::reg::set_value_dword(HKEY_CURRENT_USER, testSubkey, dwordValueName, test_dword_two);
        const wil::unique_hkey readonly{wil::reg::open_unique_key(HKEY_CURRENT_USER, testSubkey, wil::reg::key_access{KEY_QUERY_VALUE})};
        REQUIRE(wil::reg::get_value_dword(readonly.get(), dwordValueName) == test_dword_two);
    }
#endif
}

namespace
{
// RegSetValueExW accepts an odd byte count for REG_SZ. RegGetValueW reports that size plus room for a null (23 -> 25),
// but string types can only allocate whole WCHARs, so they round that request up (26 bytes). The value then grows past
// that buffer (29 -> 31) to force another resize. The expanded-string readers accept REG_SZ too, so they see the same
// odd sizes; this growing REG_EXPAND_SZ is reported at its expanded (even) size.
constexpr auto* oddSizeStringValueName = L"OddSizeStringValue";
constexpr wchar_t oddSizeStringData[] = L"abcdefghijklmnop";
constexpr DWORD oddSizeShortBytes = 23; // 11 WCHARs + 1 byte
constexpr DWORD oddSizeGrownBytes = 29; // 14 WCHARs + 1 byte
constexpr PCWSTR oddSizeExpectedResult = L"abcdefghijklmn";

// REG_MULTI_SZ data whose odd-sized prefixes end mid-WCHAR in the last string: 23 bytes is "abcd\0efgh\0i" + 1 byte,
// 29 bytes is "abcd\0efgh\0ijkl" + 1 byte. Its readers use byte buffers, which allocate exactly the bytes requested.
constexpr wchar_t oddSizeMultiStringData[] = L"abcd\0efgh\0ijklmnop";
// RegGetValueW drops the odd trailing byte and double-null-terminates the result (32 bytes, including the implicit null)
constexpr wchar_t oddSizeMultiStringExpectedResult[] = L"abcd\0efgh\0ijkl\0";

// the public multi-string readers allocate their byte buffer internally, where the detour can't observe its size
constexpr DWORD skipAdvertisedBytesCheck = MAXDWORD;

// Grows an odd-sized REG_SZ, REG_EXPAND_SZ, or REG_MULTI_SZ between WIL's size query and its read, verifying WIL
// advertises exactly the bytes it allocated: RegGetValueW copies up to the advertised size before it returns ERROR_MORE_DATA.
template <typename T, typename GetFn, typename ExpectedBytesFn>
void verify_odd_size_string_growing_during_read(
    DWORD valueType, T& result, GetFn&& getValue, ExpectedBytesFn&& expectedAdvertisedBytes, const wchar_t* valueData = oddSizeStringData)
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
    REQUIRE(
        ERROR_SUCCESS ==
        ::RegSetValueExW(hkey.get(), oddSizeStringValueName, 0, valueType, reinterpret_cast<const BYTE*>(valueData), oddSizeShortBytes));

    uint32_t callCount = 0;
    uint32_t buffersChecked = 0;
    DWORD mismatchedAdvertisedBytes = 0;
    DWORD mismatchedExpectedBytes = 0;
    witest::detoured_thread_function<&::RegGetValueW> detour;
    REQUIRE_SUCCEEDED(detour.reset([&](HKEY key, PCWSTR subkey, PCWSTR valueName, DWORD flags, DWORD* type, void* data, DWORD* dataBytes) -> LSTATUS {
        ++callCount;
        if (callCount == 2)
        {
            ::RegSetValueExW(hkey.get(), oddSizeStringValueName, 0, valueType, reinterpret_cast<const BYTE*>(valueData), oddSizeGrownBytes);
        }
        if (data)
        {
            ++buffersChecked;
            const DWORD expectedBytes = expectedAdvertisedBytes(result, callCount);
            if ((expectedBytes != skipAdvertisedBytesCheck) && (*dataBytes != expectedBytes) && (mismatchedAdvertisedBytes == 0))
            {
                mismatchedAdvertisedBytes = *dataBytes;
                mismatchedExpectedBytes = expectedBytes;
            }
        }
        return ::RegGetValueW(key, subkey, valueName, flags, type, data, dataBytes);
    }));

    REQUIRE_SUCCEEDED(getValue(hkey.get(), result));
    REQUIRE(callCount > 2);
    REQUIRE(buffersChecked > 0);
    REQUIRE(mismatchedAdvertisedBytes == mismatchedExpectedBytes);
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::odd_size_string_growing_during_read
// Coverage: Odd-byte SZ/EXPAND/MULTI growth between query/read forces retries; direct scalar/raw buffers advertise
// their allocated byte capacities, while MULTI parsers separately verify final elements.
// OLEAUTO BSTR and OBJBASE CoTaskMem use named nothrow reads; exception-enabled wstring/byte-vector reg_view
// and MULTI vector get cases verify trimmed scalar contents or raw/parsed MULTI results.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::odd_size_string_growing_during_read", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

#if defined(__WIL_OLEAUTO_H_)
    SECTION("unique_bstr")
    {
        wil::unique_bstr result;
        verify_odd_size_string_growing_during_read(
            REG_SZ,
            result,
            [](HKEY key, wil::unique_bstr& output) {
                return wil::reg::get_value_string_nothrow(key, oddSizeStringValueName, output);
            },
            [](const wil::unique_bstr& value, uint32_t) -> DWORD {
                // the BSTR's characters plus its null terminator
                return value ? ::SysStringByteLen(value.get()) + sizeof(WCHAR) : 0;
            });
        REQUIRE(AreStringsEqual(result, oddSizeExpectedResult));
    }

    SECTION("unique_bstr expanded string")
    {
        for (const DWORD storedType : {REG_SZ, REG_EXPAND_SZ})
        {
            INFO("stored type " << storedType);
            wil::unique_bstr result;
            verify_odd_size_string_growing_during_read(
                storedType,
                result,
                [](HKEY key, wil::unique_bstr& output) {
                    return wil::reg::get_value_expanded_string_nothrow(key, oddSizeStringValueName, output);
                },
                [](const wil::unique_bstr& value, uint32_t) -> DWORD {
                    return value ? ::SysStringByteLen(value.get()) + sizeof(WCHAR) : 0;
                });
            REQUIRE(AreStringsEqual(result, oddSizeExpectedResult));
        }
    }
    SECTION("unique_bstr expanded string")
    {
        for (const DWORD storedType : {REG_SZ, REG_EXPAND_SZ})
        {
            INFO("stored type " << storedType);
            wil::unique_bstr result;
            verify_odd_size_string_growing_during_read(
                storedType,
                result,
                [](HKEY key, wil::unique_bstr& output) {
                    return wil::reg::get_value_expanded_string_nothrow(key, oddSizeStringValueName, output);
                },
                [](const wil::unique_bstr& value, uint32_t) -> DWORD {
                    return value ? ::SysStringByteLen(value.get()) + sizeof(WCHAR) : 0;
                });
            REQUIRE(AreStringsEqual(result, oddSizeExpectedResult));
        }
    }
#endif

#if defined(__WIL_OBJBASE_H_)
    SECTION("unique_cotaskmem_string")
    {
        wil::com_ptr_nothrow<IMalloc> malloc;
        REQUIRE_SUCCEEDED(::CoGetMalloc(1, &malloc));

        wil::unique_cotaskmem_string result;
        verify_odd_size_string_growing_during_read(
            REG_SZ,
            result,
            [](HKEY key, wil::unique_cotaskmem_string& output) {
                return wil::reg::get_value_string_nothrow(key, oddSizeStringValueName, output);
            },
            [&](const wil::unique_cotaskmem_string& value, uint32_t) -> DWORD {
                return value ? static_cast<DWORD>(malloc->GetSize(value.get())) : 0;
            });
        REQUIRE(0 == wcscmp(result.get(), oddSizeExpectedResult));
    }

    SECTION("unique_cotaskmem_string expanded string")
    {
        wil::com_ptr_nothrow<IMalloc> malloc;
        REQUIRE_SUCCEEDED(::CoGetMalloc(1, &malloc));

        for (const DWORD storedType : {REG_SZ, REG_EXPAND_SZ})
        {
            INFO("stored type " << storedType);
            wil::unique_cotaskmem_string result;
            verify_odd_size_string_growing_during_read(
                storedType,
                result,
                [](HKEY key, wil::unique_cotaskmem_string& output) {
                    return wil::reg::get_value_expanded_string_nothrow(key, oddSizeStringValueName, output);
                },
                [&](const wil::unique_cotaskmem_string& value, uint32_t) -> DWORD {
                    return value ? static_cast<DWORD>(malloc->GetSize(value.get())) : 0;
                });
            REQUIRE(0 == wcscmp(result.get(), oddSizeExpectedResult));
        }
    }
#endif

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("std::wstring")
    {
        std::wstring result;
        verify_odd_size_string_growing_during_read(
            REG_SZ,
            result,
            [](HKEY key, std::wstring& output) {
                // reg_view reads directly into the caller's string, so the detour can observe its buffer
                wil::reg::reg_view_details::reg_view{key}.get_value(nullptr, oddSizeStringValueName, output);
                return S_OK;
            },
            [](const std::wstring& value, uint32_t callNumber) -> DWORD {
                // the first call advertises the empty string's null slot; after a resize, the null slot is not advertised
                return static_cast<DWORD>((callNumber == 1 ? value.size() + 1 : value.size()) * sizeof(wchar_t));
            });
        REQUIRE(result == oddSizeExpectedResult);
    }

    SECTION("std::wstring expanded string")
    {
        for (const DWORD storedType : {REG_SZ, REG_EXPAND_SZ})
        {
            INFO("stored type " << storedType);
            std::wstring result;
            verify_odd_size_string_growing_during_read(
                storedType,
                result,
                [](HKEY key, std::wstring& output) {
                    // the same reg_view call wil::reg::get_value_expanded_string makes, reading directly into the caller's string
                    wil::reg::reg_view_details::reg_view{key}.get_value(nullptr, oddSizeStringValueName, output, REG_EXPAND_SZ);
                    return S_OK;
                },
                [](const std::wstring& value, uint32_t callNumber) -> DWORD {
                    return static_cast<DWORD>((callNumber == 1 ? value.size() + 1 : value.size()) * sizeof(wchar_t));
                });
            REQUIRE(result == oddSizeExpectedResult);
        }
    }

    SECTION("std::vector<uint8_t> multistring")
    {
        std::vector<uint8_t> result;
        verify_odd_size_string_growing_during_read(
            REG_MULTI_SZ,
            result,
            [](HKEY key, std::vector<uint8_t>& output) {
                // the same reg_view call wil::reg::get_value_multistring makes through get_value_binary
                wil::reg::reg_view_details::reg_view{key}.get_value(nullptr, oddSizeStringValueName, output, REG_MULTI_SZ);
                return S_OK;
            },
            [](const std::vector<uint8_t>& value, uint32_t) -> DWORD {
                return static_cast<DWORD>(value.size());
            },
            oddSizeMultiStringData);
        REQUIRE(result.size() == sizeof(oddSizeMultiStringExpectedResult));
        REQUIRE(0 == memcmp(result.data(), oddSizeMultiStringExpectedResult, sizeof(oddSizeMultiStringExpectedResult)));
    }

    SECTION("std::vector<std::wstring> multistring")
    {
        std::vector<std::wstring> result;
        verify_odd_size_string_growing_during_read(
            REG_MULTI_SZ,
            result,
            [](HKEY key, std::vector<std::wstring>& output) {
                output = wil::reg::get_value_multistring(key, oddSizeStringValueName);
                return S_OK;
            },
            [](const std::vector<std::wstring>&, uint32_t) -> DWORD {
                return skipAdvertisedBytesCheck;
            },
            oddSizeMultiStringData);
        REQUIRE(result == std::vector<std::wstring>{L"abcd", L"efgh", L"ijkl"});
    }
#endif

#if defined(__WIL_OBJBASE_H_)
    SECTION("unique_cotaskmem_array_ptr<uint8_t> multistring")
    {
        wil::unique_cotaskmem_array_ptr<uint8_t> result;
        verify_odd_size_string_growing_during_read(
            REG_MULTI_SZ,
            result,
            [](HKEY key, wil::unique_cotaskmem_array_ptr<uint8_t>& output) {
                // the same call wil::reg::get_value_multistring_nothrow makes
                return wil::reg::get_value_binary_nothrow(key, oddSizeStringValueName, REG_MULTI_SZ, output);
            },
            [](const wil::unique_cotaskmem_array_ptr<uint8_t>& value, uint32_t) -> DWORD {
                return static_cast<DWORD>(value.size());
            },
            oddSizeMultiStringData);
        REQUIRE(result.size() == sizeof(oddSizeMultiStringExpectedResult));
        REQUIRE(0 == memcmp(result.get(), oddSizeMultiStringExpectedResult, sizeof(oddSizeMultiStringExpectedResult)));
    }

    SECTION("unique_cotaskmem_array_ptr<unique_cotaskmem_string> multistring")
    {
        wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> result;
        verify_odd_size_string_growing_during_read(
            REG_MULTI_SZ,
            result,
            [](HKEY key, wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string>& output) {
                return wil::reg::get_value_multistring_nothrow(key, oddSizeStringValueName, output);
            },
            [](const wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string>&, uint32_t) -> DWORD {
                return skipAdvertisedBytesCheck;
            },
            oddSizeMultiStringData);
        REQUIRE(result.size() == 3);
        REQUIRE(0 == wcscmp(result[0], L"abcd"));
        REQUIRE(0 == wcscmp(result[1], L"efgh"));
        REQUIRE(0 == wcscmp(result[2], L"ijkl"));
    }
#endif
}

namespace
{
// RegGetValueW cb is bytes; whole-WCHAR read capacity rounds up, since rounding down can cause nonconverging odd-size reads.
constexpr auto* oddSizeStableExpandValueName = L"OddSizeStableExpandValue";
constexpr wchar_t oddSizeStableExpandData[] = L"ab\0x";
constexpr DWORD oddSizeStableExpandBytes = 7;
constexpr uint32_t oddSizeStableMaxReadCalls = 10;

template <typename T, typename GetFn>
void verify_odd_size_stable_expanded_read(T& result, GetFn&& getValue)
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
    REQUIRE(
        ERROR_SUCCESS ==
        ::RegSetValueExW(
            hkey.get(), oddSizeStableExpandValueName, 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE*>(oddSizeStableExpandData), oddSizeStableExpandBytes));

    uint32_t callCount = 0;
    witest::detoured_thread_function<&::RegGetValueW> detour;
    REQUIRE_SUCCEEDED(
        detour.reset([&](HKEY key, PCWSTR subkey, PCWSTR valueName, DWORD flags, DWORD* type, void* data, DWORD* dataBytes) -> LSTATUS {
            // fail instead of hanging if the read never converges
            if (++callCount > oddSizeStableMaxReadCalls)
            {
                return ERROR_INVALID_DATA;
            }
            return ::RegGetValueW(key, subkey, valueName, flags, type, data, dataBytes);
        }));

    REQUIRE_SUCCEEDED(getValue(hkey.get(), result));
    REQUIRE(callCount <= oddSizeStableMaxReadCalls);
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::odd_size_stable_expanded_string_read
// Coverage: Stable odd-byte REG_EXPAND_SZ reads converge within a bounded call count and return trimmed content;
// named nothrow readers use OLEAUTO BSTR/OBJBASE CoTaskMem, with named wstring get only when exceptions are enabled.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::odd_size_stable_expanded_string_read", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

#if defined(__WIL_OLEAUTO_H_)
    SECTION("unique_bstr")
    {
        wil::unique_bstr result;
        verify_odd_size_stable_expanded_read(result, [](HKEY key, wil::unique_bstr& output) {
            return wil::reg::get_value_expanded_string_nothrow(key, oddSizeStableExpandValueName, output);
        });
        REQUIRE(AreStringsEqual(result, L"ab"));
    }
#endif

#if defined(__WIL_OBJBASE_H_)
    SECTION("unique_cotaskmem_string")
    {
        wil::unique_cotaskmem_string result;
        verify_odd_size_stable_expanded_read(result, [](HKEY key, wil::unique_cotaskmem_string& output) {
            return wil::reg::get_value_expanded_string_nothrow(key, oddSizeStableExpandValueName, output);
        });
        REQUIRE(0 == wcscmp(result.get(), L"ab"));
    }
#endif

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("std::wstring")
    {
        std::wstring result;
        verify_odd_size_stable_expanded_read(result, [](HKEY key, std::wstring& output) {
            output = wil::reg::get_value_expanded_string(key, oddSizeStableExpandValueName);
            return S_OK;
        });
        REQUIRE(result == L"ab");
    }
#endif
}
namespace
{
// Read capacity covers the full byte extent, but an incomplete final WCHAR is not owned string content.
constexpr auto* oddSizeTrailingByteValueName = L"OddSizeTrailingByteValue";
constexpr wchar_t oddSizeTrailingByteData[] = L"abcdefghijklmnop";
constexpr DWORD oddSizeTrailingByteStoredBytes = 25;
constexpr wchar_t oddSizeTrailingByteExpected[] = L"abcdefghijkl";
constexpr size_t oddSizeTrailingByteExpectedLength = ARRAYSIZE(oddSizeTrailingByteExpected) - 1;
constexpr uint32_t oddSizeTrailingByteMaxReadCalls = 10;

template <typename T, typename GetFn, typename DataFn, typename LengthFn>
void verify_odd_size_string_drops_trailing_byte(T& result, GetFn&& getValue, DataFn&& data, LengthFn&& length)
{
    wil::unique_hkey hkey;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, hkey, wil::reg::key_access::readwrite));
    REQUIRE(
        ERROR_SUCCESS ==
        ::RegSetValueExW(
            hkey.get(), oddSizeTrailingByteValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(oddSizeTrailingByteData), oddSizeTrailingByteStoredBytes));

    uint32_t callCount = 0;
    DWORD successfulReadAdvertisedBytes = 0;
    witest::detoured_thread_function<&::RegGetValueW> detour;
    REQUIRE_SUCCEEDED(detour.reset(
        [&](HKEY key, PCWSTR subkey, PCWSTR valueName, DWORD flags, DWORD* type, void* buffer, DWORD* dataBytes) -> LSTATUS {
            // fail instead of hanging if the read never converges
            if (++callCount > oddSizeTrailingByteMaxReadCalls)
            {
                return ERROR_INVALID_DATA;
            }
            const DWORD advertisedBytes = dataBytes ? *dataBytes : 0;
            const auto status = ::RegGetValueW(key, subkey, valueName, flags, type, buffer, dataBytes);
            if ((status == ERROR_SUCCESS) && buffer)
            {
                successfulReadAdvertisedBytes = advertisedBytes;
            }
            return status;
        }));

    REQUIRE_SUCCEEDED(getValue(hkey.get(), result));
    // the read that succeeded had room for every stored byte, including the odd trailing byte
    REQUIRE(successfulReadAdvertisedBytes >= oddSizeTrailingByteStoredBytes);
    // but only the whole WCHARs are returned
    REQUIRE(length(result) == oddSizeTrailingByteExpectedLength);
    const wchar_t* const resultData = data(result);
    REQUIRE(resultData != nullptr);
    REQUIRE(0 == memcmp(resultData, oddSizeTrailingByteExpected, sizeof(oddSizeTrailingByteExpected)));
}
} // namespace

namespace
{
constexpr auto* malformedStringValueName = L"AlignedNullTrailingByte";
constexpr BYTE malformedStringBytes[]{'a', 0, 'b', 0, 'c', 0, 0, 0, 0x41};

wil::unique_hkey create_malformed_string_key(DWORD type)
{
    wil::unique_hkey key;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, key, wil::reg::key_access::readwrite));
    REQUIRE(
        ERROR_SUCCESS ==
        ::RegSetValueExW(key.get(), malformedStringValueName, 0, type, malformedStringBytes, sizeof(malformedStringBytes)));
    return key;
}

template <typename T, typename DataFn, typename LengthFn>
void verify_aligned_null_trailing_byte(T& result, DataFn data, LengthFn length)
{
    for (const DWORD type : {REG_SZ, REG_EXPAND_SZ})
    {
        INFO("registry type = " << type);
        const auto key = create_malformed_string_key(type);
        const wil::reg::reg_view_details::reg_view_nothrow view{key.get()};
        REQUIRE_SUCCEEDED(view.get_value(nullptr, malformedStringValueName, result, type));
        const auto* buffer = data(result);
        REQUIRE(buffer != nullptr);
        const size_t count = length(result);
        // Owned-buffer invariant, separate from the native byte characterization below.
        REQUIRE(buffer[count] == L'\0');
        REQUIRE(count == 3);
        REQUIRE(::wcsnlen(buffer, count + 1) == count);
        REQUIRE(0 == memcmp(buffer, L"abc", 3 * sizeof(wchar_t)));
        REQUIRE(buffer[3] == L'\0');
    }
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::malformed_string_aligned_null_trailing_byte
// Coverage: SZ/EXPAND aligned-null plus partial-WCHAR bytes distinguish native RegGetValueW characterization
// from reg_view_nothrow owned length/end-null checks: process heap and OLEAUTO/OBJBASE unique/shared strings.
// Shared owners and STL wstring require exceptions; guarded named get/try and preallocated cases verify content
// and termination without claiming universal native byte normalization.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::malformed_string_aligned_null_trailing_byte", "[registry]")
{
    // RegGetValueW can keep cb unchanged and leave a nonzero trailing byte after an aligned null.
    // Native byte behavior does not establish WIL's owned-buffer end-null invariant.
    SECTION("RegGetValueW byte characterization")
    {
        for (const DWORD type : {REG_SZ, REG_EXPAND_SZ})
        {
            const auto key = create_malformed_string_key(type);
            WCHAR buffer[8];
            for (auto& character : buffer)
            {
                character = 0xcccc;
            }
            DWORD bytes = sizeof(buffer);
            DWORD returnedType = 0;
            const auto status = ::RegGetValueW(
                key.get(),
                nullptr,
                malformedStringValueName,
                wil::reg::reg_view_details::get_value_flags_from_value_type(type),
                &returnedType,
                buffer,
                &bytes);
            INFO("stored type = " << type << "; returned type = " << returnedType << "; returned bytes = " << bytes
                 << "; trailing WCHAR = " << static_cast<unsigned int>(buffer[4]));
            REQUIRE(status == ERROR_SUCCESS);
            REQUIRE(buffer[3] == L'\0');
            REQUIRE(0 == memcmp(buffer, L"abc", 3 * sizeof(wchar_t)));
        }
    }
#if defined(__WIL_OLEAUTO_H_)
    SECTION("unique_bstr")
    {
        wil::unique_bstr value;
        verify_aligned_null_trailing_byte(
            value,
            [](const auto& v) { return v.get(); },
            [](const auto& v) { return ::SysStringLen(v.get()); });
    }
    SECTION("preallocated BSTR")
    {
        wil::unique_bstr value{::SysAllocStringLen(nullptr, 4)};
        REQUIRE(value != nullptr);
        verify_aligned_null_trailing_byte(
            value,
            [](const auto& v) { return v.get(); },
            [](const auto& v) { return ::SysStringLen(v.get()); });
    }
#endif
#if defined(__WIL_OLEAUTO_H_STL)
    SECTION("shared_bstr")
    {
        wil::shared_bstr value;
        verify_aligned_null_trailing_byte(
            value,
            [](const auto& v) { return v.get(); },
            [](const auto& v) { return ::SysStringLen(v.get()); });
    }
#endif
#if defined(__WIL_OBJBASE_H_)
    SECTION("unique_cotaskmem_string")
    {
        wil::unique_cotaskmem_string value;
        verify_aligned_null_trailing_byte(
            value,
            [](const auto& v) { return v.get(); },
            [](const auto&) { return 3u; });
    }
#endif
#if defined(__WIL_OBJBASE_H_STL)
    SECTION("shared_cotaskmem_string")
    {
        wil::shared_cotaskmem_string value;
        verify_aligned_null_trailing_byte(
            value,
            [](const auto& v) { return v.get(); },
            [](const auto&) { return 3u; });
    }
#endif
    SECTION("unique_process_heap_string")
    {
        wil::unique_process_heap_string value;
        verify_aligned_null_trailing_byte(
            value,
            [](const auto& v) { return v.get(); },
            [](const auto&) { return 3u; });
    }
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("std::wstring")
    {
        std::wstring value;
        verify_aligned_null_trailing_byte(
            value,
            [](const auto& v) { return v.c_str(); },
            [](const auto& v) { return v.size(); });
    }
    SECTION("preallocated std::wstring")
    {
        std::wstring value(4, L'x');
        verify_aligned_null_trailing_byte(
            value,
            [](const auto& v) { return v.c_str(); },
            [](const auto& v) { return v.size(); });
    }
    SECTION("try_get_value_string")
    {
        const auto key = create_malformed_string_key(REG_SZ);
        const auto value = wil::reg::try_get_value_string(key.get(), malformedStringValueName);
        REQUIRE(value.has_value());
        REQUIRE(value->c_str()[value->size()] == L'\0');
        REQUIRE(*value == L"abc");
    }
    SECTION("get_value_string")
    {
        const auto key = create_malformed_string_key(REG_SZ);
        const auto value = wil::reg::get_value_string(key.get(), malformedStringValueName);
        REQUIRE(value.c_str()[value.size()] == L'\0');
        REQUIRE(value == L"abc");
    }
    SECTION("get_value_expanded_string")
    {
        const auto key = create_malformed_string_key(REG_EXPAND_SZ);
        const auto value = wil::reg::get_value_expanded_string(key.get(), malformedStringValueName);
        REQUIRE(value.c_str()[value.size()] == L'\0');
        REQUIRE(value == L"abc");
    }
#endif
}

namespace
{
constexpr auto* embeddedNullTrailingByteValueName = L"EmbeddedNullTrailingByte";
const std::vector<BYTE> embeddedNullTrailingBytes{'h', 0, 'i', 0, 0, 0, '!'};

wil::unique_hkey create_embedded_null_trailing_byte_key(DWORD type)
{
    wil::unique_hkey key;
    REQUIRE_SUCCEEDED(wil::reg::create_unique_key_nothrow(HKEY_CURRENT_USER, testSubkey, key, wil::reg::key_access::readwrite));
    REQUIRE(
        ERROR_SUCCESS == set_value_exact_bytes(
                             key.get(),
                             embeddedNullTrailingByteValueName,
                             type,
                             embeddedNullTrailingBytes,
                             static_cast<DWORD>(embeddedNullTrailingBytes.size())));
    return key;
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::odd_size_string_embedded_null_trailing_byte
// Coverage: a REG_SZ/REG_EXPAND_SZ value of L"hi", an aligned null, and one extra byte is stored as exactly 7 bytes;
// RegGetValueW advertises 9 bytes, rejects a 6-byte buffer, and successfully returns all 7 raw bytes (an odd count,
// including the extra byte) for 7- to 10-byte buffers. WIL string readers return "hi" with an owned end null.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::odd_size_string_embedded_null_trailing_byte", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }
    const auto storedBytes = static_cast<DWORD>(embeddedNullTrailingBytes.size());

    for (const DWORD type : {REG_SZ, REG_EXPAND_SZ})
    {
        INFO("registry type = " << type);
        const auto key = create_embedded_null_trailing_byte_key(type);
        const DWORD flags = type == REG_SZ ? RRF_RT_REG_SZ : (RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND);

        BYTE stored[16];
        memset(stored, 0xcc, sizeof(stored));
        DWORD bytes = sizeof(stored);
        DWORD returnedType = REG_NONE;
        REQUIRE(ERROR_SUCCESS == ::RegQueryValueExW(key.get(), embeddedNullTrailingByteValueName, nullptr, &returnedType, stored, &bytes));
        REQUIRE(returnedType == type);
        REQUIRE(bytes == storedBytes);
        REQUIRE(0 == memcmp(stored, embeddedNullTrailingBytes.data(), storedBytes));

        bytes = 0;
        REQUIRE(ERROR_SUCCESS == ::RegGetValueW(key.get(), nullptr, embeddedNullTrailingByteValueName, flags, &returnedType, nullptr, &bytes));
        REQUIRE(bytes == storedBytes + sizeof(wchar_t));

        for (DWORD capacity = storedBytes - 1; capacity <= storedBytes + 3; ++capacity)
        {
            INFO("buffer bytes = " << capacity);
            BYTE buffer[16];
            memset(buffer, 0xcc, sizeof(buffer));
            bytes = capacity;
            const auto status =
                ::RegGetValueW(key.get(), nullptr, embeddedNullTrailingByteValueName, flags, &returnedType, buffer, &bytes);
            if (capacity < storedBytes)
            {
                REQUIRE(status == ERROR_MORE_DATA);
                REQUIRE(bytes == storedBytes + sizeof(wchar_t));
                REQUIRE(buffer[0] == 0xcc);
                continue;
            }
            REQUIRE(status == ERROR_SUCCESS);
            REQUIRE(returnedType == type);
            REQUIRE(bytes == storedBytes);
            REQUIRE(0 == memcmp(buffer, embeddedNullTrailingBytes.data(), storedBytes));
            for (DWORD index = storedBytes; index < sizeof(buffer); ++index)
            {
                REQUIRE(buffer[index] == 0xcc);
            }
        }

#if defined(__WIL_OLEAUTO_H_)
        {
            wil::unique_bstr value;
            REQUIRE_SUCCEEDED(
                type == REG_SZ ? wil::reg::get_value_string_nothrow(key.get(), embeddedNullTrailingByteValueName, value)
                               : wil::reg::get_value_expanded_string_nothrow(key.get(), embeddedNullTrailingByteValueName, value));
            REQUIRE(AreStringsEqual(value, L"hi"));
        }
#endif
#if defined(__WIL_OBJBASE_H_)
        {
            wil::unique_cotaskmem_string value;
            REQUIRE_SUCCEEDED(
                type == REG_SZ ? wil::reg::get_value_string_nothrow(key.get(), embeddedNullTrailingByteValueName, value)
                               : wil::reg::get_value_expanded_string_nothrow(key.get(), embeddedNullTrailingByteValueName, value));
            REQUIRE(value != nullptr);
            REQUIRE(0 == wcscmp(value.get(), L"hi"));
        }
#endif
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
        {
            const auto value = type == REG_SZ ? wil::reg::get_value_string(key.get(), embeddedNullTrailingByteValueName)
                                              : wil::reg::get_value_expanded_string(key.get(), embeddedNullTrailingByteValueName);
            REQUIRE(value.c_str()[value.size()] == L'\0');
            REQUIRE(value == L"hi");
        }
#endif
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::malformed_multistring_aligned_null_trailing_byte
// Coverage: REG_MULTI_SZ single/double-null endings followed by a partial WCHAR yield two terminated elements;
// OBJBASE named CoTaskMem nothrow reads verify bounded lengths/content, while exception-enabled STL vector
// named get/try reads verify both distinct payloads and owned element terminators.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::malformed_multistring_aligned_null_trailing_byte", "[registry]")
{
    // First string has an aligned null; the second has no final double null, followed by a nonzero partial WCHAR.
    const BYTE missingDoubleNull[]{'a', 0, 'b', 0, 'c', 0, 0, 0, 'd', 0, 'e', 0, 'f', 0, 0, 0, 0x41};
    const BYTE doubleNull[]{'a', 0, 'b', 0, 'c', 0, 0, 0, 'd', 0, 'e', 0, 'f', 0, 0, 0, 0, 0, 0x41};
    for (const auto& bytes : {wil::make_range(missingDoubleNull, sizeof(missingDoubleNull)), wil::make_range(doubleNull, sizeof(doubleNull))})
    {
        DYNAMIC_SECTION("stored bytes = " << (bytes.end() - bytes.begin()))
        {
            INFO("stored bytes = " << (bytes.end() - bytes.begin()));
            const auto key = create_malformed_string_key(REG_MULTI_SZ);
            REQUIRE(
                ERROR_SUCCESS ==
                ::RegSetValueExW(
                    key.get(), malformedStringValueName, 0, REG_MULTI_SZ, bytes.begin(), static_cast<DWORD>(bytes.end() - bytes.begin())));
#if defined(__WIL_OBJBASE_H_)
            SECTION("get_value_multistring_nothrow")
            {
                wil::unique_cotaskmem_array_ptr<wil::unique_cotaskmem_string> value;
                REQUIRE_SUCCEEDED(wil::reg::get_value_multistring_nothrow(key.get(), malformedStringValueName, value));
                REQUIRE(value.size() == 2);
                for (size_t i = 0; i < value.size(); ++i)
                {
                    REQUIRE(value[i] != nullptr);
                    REQUIRE(value[i][3] == L'\0');
                    REQUIRE(::wcsnlen(value[i], 4) == 3);
                    REQUIRE(0 == memcmp(value[i], i == 0 ? L"abc" : L"def", 3 * sizeof(wchar_t)));
                }
            }
#endif
#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS)
            SECTION("get_value_multistring")
            {
                const auto value = wil::reg::get_value_multistring(key.get(), malformedStringValueName);
                REQUIRE(value.size() == 2);
                for (size_t i = 0; i < value.size(); ++i)
                {
                    REQUIRE(value[i].c_str()[value[i].size()] == L'\0');
                    REQUIRE(value[i] == (i == 0 ? L"abc" : L"def"));
                }
            }
            SECTION("try_get_value_multistring")
            {
                const auto value = wil::reg::try_get_value_multistring(key.get(), malformedStringValueName);
                REQUIRE(value.has_value());
                REQUIRE(value->size() == 2);
                for (size_t i = 0; i < value->size(); ++i)
                {
                    REQUIRE((*value)[i].c_str()[(*value)[i].size()] == L'\0');
                    REQUIRE((*value)[i] == (i == 0 ? L"abc" : L"def"));
                }
            }
#endif
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// BasicRegistryTests::odd_size_string_drops_trailing_byte
// Coverage: REG_SZ odd-byte reads have capacity for all stored bytes but return only complete WCHAR content;
// literal/expanded named nothrow reads use fixed WCHAR, OLEAUTO BSTR and OBJBASE CoTaskMem.
// Shared literal readers and named wstring literal/expanded get cases require exceptions and their adapter guards.
//----------------------------------------------------------------------------------------------------------------------
TEST_CASE("BasicRegistryTests::odd_size_string_drops_trailing_byte", "[registry]")
{
    const auto deleteHr = HRESULT_FROM_WIN32(::RegDeleteTreeW(HKEY_CURRENT_USER, testSubkey));
    if (deleteHr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        REQUIRE_SUCCEEDED(deleteHr);
    }

    SECTION("WCHAR array")
    {
        WCHAR result[ARRAYSIZE(oddSizeTrailingByteData)]{};
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, WCHAR(&output)[ARRAYSIZE(oddSizeTrailingByteData)]) {
                return wil::reg::get_value_string_nothrow(key, oddSizeTrailingByteValueName, output);
            },
            [](const WCHAR* value) {
                return value;
            },
            [](const WCHAR* value) {
                return wcslen(value);
            });
    }

    SECTION("WCHAR array expanded string")
    {
        WCHAR result[ARRAYSIZE(oddSizeTrailingByteData)]{};
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, WCHAR(&output)[ARRAYSIZE(oddSizeTrailingByteData)]) {
                return wil::reg::get_value_expanded_string_nothrow(key, oddSizeTrailingByteValueName, output);
            },
            [](const WCHAR* value) {
                return value;
            },
            [](const WCHAR* value) {
                return wcslen(value);
            });
    }

#if defined(__WIL_OLEAUTO_H_)
    SECTION("unique_bstr")
    {
        wil::unique_bstr result;
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, wil::unique_bstr& output) {
                return wil::reg::get_value_string_nothrow(key, oddSizeTrailingByteValueName, output);
            },
            [](const wil::unique_bstr& value) {
                return static_cast<const wchar_t*>(value.get());
            },
            [](const wil::unique_bstr& value) {
                return static_cast<size_t>(::SysStringLen(value.get()));
            });
    }

    SECTION("unique_bstr expanded string")
    {
        wil::unique_bstr result;
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, wil::unique_bstr& output) {
                return wil::reg::get_value_expanded_string_nothrow(key, oddSizeTrailingByteValueName, output);
            },
            [](const wil::unique_bstr& value) {
                return static_cast<const wchar_t*>(value.get());
            },
            [](const wil::unique_bstr& value) {
                return static_cast<size_t>(::SysStringLen(value.get()));
            });
    }
    SECTION("unique_bstr expanded string")
    {
        wil::unique_bstr result;
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, wil::unique_bstr& output) {
                return wil::reg::get_value_expanded_string_nothrow(key, oddSizeTrailingByteValueName, output);
            },
            [](const wil::unique_bstr& value) {
                return static_cast<const wchar_t*>(value.get());
            },
            [](const wil::unique_bstr& value) {
                return static_cast<size_t>(::SysStringLen(value.get()));
            });
    }
#endif

#if defined(__WIL_OLEAUTO_H_STL)
    SECTION("shared_bstr")
    {
        wil::shared_bstr result;
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, wil::shared_bstr& output) {
                return wil::reg::get_value_string_nothrow(key, oddSizeTrailingByteValueName, output);
            },
            [](const wil::shared_bstr& value) {
                return static_cast<const wchar_t*>(value.get());
            },
            [](const wil::shared_bstr& value) {
                return static_cast<size_t>(::SysStringLen(value.get()));
            });
    }
#endif

#if defined(__WIL_OBJBASE_H_)
    SECTION("unique_cotaskmem_string")
    {
        wil::unique_cotaskmem_string result;
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, wil::unique_cotaskmem_string& output) {
                return wil::reg::get_value_string_nothrow(key, oddSizeTrailingByteValueName, output);
            },
            [](const wil::unique_cotaskmem_string& value) {
                return static_cast<const wchar_t*>(value.get());
            },
            [](const wil::unique_cotaskmem_string& value) {
                return wcslen(value.get());
            });
    }

    SECTION("unique_cotaskmem_string expanded string")
    {
        wil::unique_cotaskmem_string result;
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, wil::unique_cotaskmem_string& output) {
                return wil::reg::get_value_expanded_string_nothrow(key, oddSizeTrailingByteValueName, output);
            },
            [](const wil::unique_cotaskmem_string& value) {
                return static_cast<const wchar_t*>(value.get());
            },
            [](const wil::unique_cotaskmem_string& value) {
                return wcslen(value.get());
            });
    }
#endif

#if defined(__WIL_OBJBASE_H_STL)
    SECTION("shared_cotaskmem_string")
    {
        wil::shared_cotaskmem_string result;
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, wil::shared_cotaskmem_string& output) {
                return wil::reg::get_value_string_nothrow(key, oddSizeTrailingByteValueName, output);
            },
            [](const wil::shared_cotaskmem_string& value) {
                return static_cast<const wchar_t*>(value.get());
            },
            [](const wil::shared_cotaskmem_string& value) {
                return wcslen(value.get());
            });
    }
#endif

#if defined(WIL_ENABLE_EXCEPTIONS)
    SECTION("std::wstring")
    {
        std::wstring result;
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, std::wstring& output) {
                output = wil::reg::get_value_string(key, oddSizeTrailingByteValueName);
                return S_OK;
            },
            [](const std::wstring& value) {
                return value.c_str();
            },
            [](const std::wstring& value) {
                return value.size();
            });
    }

    SECTION("std::wstring expanded string")
    {
        std::wstring result;
        verify_odd_size_string_drops_trailing_byte(
            result,
            [](HKEY key, std::wstring& output) {
                output = wil::reg::get_value_expanded_string(key, oddSizeTrailingByteValueName);
                return S_OK;
            },
            [](const std::wstring& value) {
                return value.c_str();
            },
            [](const std::wstring& value) {
                return value.size();
            });
    }
#endif
}