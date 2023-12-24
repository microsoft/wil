// Relatively simple tests as a sanity check to verify that our funciton mocking & use of detours is working correctly

#include "common.h"

#ifdef WIL_ENABLE_EXCEPTIONS
#include <thread>
#endif

DWORD __stdcall InvertFileAttributes(PCWSTR path)
{
    thread_local bool recursive = false;
    if (recursive) return INVALID_FILE_ATTRIBUTES;

    recursive = true;
    auto result = ::GetFileAttributesW(path);
    recursive = false;

    return ~result;
}

TEST_CASE("MockingTests::ThreadDetourWithFunctionPointer", "[mocking]")
{
    wchar_t buffer[MAX_PATH];
    REQUIRE(::GetSystemDirectoryW(buffer, ARRAYSIZE(buffer)) != 0);
    auto realAttr = ::GetFileAttributesW(buffer);
    REQUIRE(realAttr != INVALID_FILE_ATTRIBUTES);
    REQUIRE(realAttr != 0);

    {
        witest::detoured_thread_function<&::GetFileAttributesW> detour;
        REQUIRE_SUCCEEDED(detour.reset(InvertFileAttributes));
        auto inverseAttr = ::GetFileAttributesW(buffer);
        REQUIRE(inverseAttr == ~realAttr);
    }

    REQUIRE(::GetFileAttributesW(buffer) == realAttr);
}

TEST_CASE("MockingTests::ThreadDetourWithLambda", "[mocking]")
{
    PCWSTR path = L"$*&><"; // Purposefully nonesense/invalid to test the mocking functionality

    {
        DWORD expectedAttr = 0;
        witest::detoured_thread_function<&::GetFileAttributesW> detour;
        REQUIRE_SUCCEEDED(detour.reset([&](PCWSTR) -> DWORD {
            return expectedAttr;
        }));

        REQUIRE(::GetFileAttributesW(path) == expectedAttr);

        expectedAttr = 0xc0ffee;
        REQUIRE(::GetFileAttributesW(path) == expectedAttr);
    }

    REQUIRE(::GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
}

__declspec(noinline) int __cdecl LocalAddFunction(int lhs, int rhs)
{
    return lhs + rhs;
}

TEST_CASE("MockingTests::ThreadDetourLocalFunciton", "[mocking]")
{
    {
        witest::detoured_thread_function<&LocalAddFunction> detour;
        REQUIRE_SUCCEEDED(detour.reset([](int lhs, int rhs) {
            return lhs * rhs;
        }));

        REQUIRE(LocalAddFunction(2, 3) == 6);
    }

    REQUIRE(LocalAddFunction(2, 3) == 5);
}

__declspec(noinline) int __cdecl LocalAddFunctionNoexcept(int lhs, int rhs) noexcept
{
    return lhs + rhs;
}

__declspec(noinline) int __stdcall LocalAddFunctionStdcallNoexcept(int lhs, int rhs) noexcept
{
    return lhs + rhs;
}

TEST_CASE("MockingTests::ThreadDetourNoexceptFunction", "[mocking]")
{
    {
        witest::detoured_thread_function<&LocalAddFunctionNoexcept> detour;
        REQUIRE_SUCCEEDED(detour.reset([](int lhs, int rhs) noexcept {
            return lhs * rhs;
        }));

        REQUIRE(LocalAddFunctionNoexcept(2, 3) == 6);
    }
    REQUIRE(LocalAddFunctionNoexcept(2, 3) == 5);

    {
        witest::detoured_thread_function<&LocalAddFunctionStdcallNoexcept> detour;
        REQUIRE_SUCCEEDED(detour.reset([](int lhs, int rhs) noexcept {
            return lhs * rhs;
        }));

        REQUIRE(LocalAddFunctionStdcallNoexcept(2, 3) == 6);
    }
    REQUIRE(LocalAddFunctionStdcallNoexcept(2, 3) == 5);
}

TEST_CASE("MockingTests::RecursiveThreadDetouring", "[mocking]")
{
    {
        witest::detoured_thread_function<&LocalAddFunction> detour;
        REQUIRE_SUCCEEDED(detour.reset([](int lhs, int rhs) {
            return lhs + rhs + LocalAddFunction(lhs * 2, rhs * 2);
        }));

        {
            witest::detoured_thread_function<&LocalAddFunction> detour2;
            REQUIRE_SUCCEEDED(detour2.reset([](int lhs, int rhs) {
                return lhs + rhs + LocalAddFunction(lhs * 3, rhs * 3);
            }));

            // Last registration should be the first to execute
            // 5 + 3 * (5 + 2 * 5)
            REQUIRE(LocalAddFunction(2, 3) == 50);
        }

        // (2 + 3) + (4 + 6)
        REQUIRE(LocalAddFunction(2, 3) == 15);
    }

    REQUIRE(LocalAddFunction(2, 3) == 5);
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("MockingTests::ThreadDetourMultithreaded", "[mocking]")
{
    witest::detoured_thread_function<&LocalAddFunction> detour([](int lhs, int rhs) {
        return lhs * rhs;
    });

    int otherThreadResult = 0;
    std::thread thread([&] {
        otherThreadResult = LocalAddFunction(2, 3);
    });
    thread.join();
    REQUIRE(otherThreadResult == 5);
}
#endif
