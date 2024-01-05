// Relatively simple tests as a sanity check to verify that our funciton mocking & use of detours is working correctly
#include "pch.h"

#include "common.h"

#ifdef WIL_ENABLE_EXCEPTIONS
#include <thread>
#endif

DWORD __stdcall InvertFileAttributes(PCWSTR path)
{
    thread_local bool recursive = false;
    if (recursive)
        return INVALID_FILE_ATTRIBUTES;

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

TEST_CASE("MockingTests::GlobalDetourWithFunctionPointer", "[mocking]")
{
    wchar_t buffer[MAX_PATH];
    REQUIRE(::GetSystemDirectoryW(buffer, ARRAYSIZE(buffer)) != 0);
    auto realAttr = ::GetFileAttributesW(buffer);
    REQUIRE(realAttr != INVALID_FILE_ATTRIBUTES);
    REQUIRE(realAttr != 0);

    {
        witest::detoured_global_function<&::GetFileAttributesW> detour;
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

TEST_CASE("MockingTests::GlobalDetourWithLambda", "[mocking]")
{
    PCWSTR path = L"$*&><"; // Purposefully nonesense/invalid to test the mocking functionality

    {
        DWORD expectedAttr = 0;
        witest::detoured_global_function<&::GetFileAttributesW> detour;
        REQUIRE_SUCCEEDED(detour.reset([&](PCWSTR) -> DWORD {
            return expectedAttr;
        }));

        REQUIRE(::GetFileAttributesW(path) == expectedAttr);

        expectedAttr = 0xc0ffee;
        REQUIRE(::GetFileAttributesW(path) == expectedAttr);
    }

    REQUIRE(::GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
}

#pragma optimize("", off) // Don't evaluate at compile-time
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

TEST_CASE("MockingTests::GlobalDetourLocalFunciton", "[mocking]")
{
    {
        witest::detoured_global_function<&LocalAddFunction> detour;
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

TEST_CASE("MockingTests::GlobalDetourNoexceptFunction", "[mocking]")
{
    {
        witest::detoured_global_function<&LocalAddFunctionNoexcept> detour;
        REQUIRE_SUCCEEDED(detour.reset([](int lhs, int rhs) noexcept {
            return lhs * rhs;
        }));

        REQUIRE(LocalAddFunctionNoexcept(2, 3) == 6);
    }
    REQUIRE(LocalAddFunctionNoexcept(2, 3) == 5);

    {
        witest::detoured_global_function<&LocalAddFunctionStdcallNoexcept> detour;
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

TEST_CASE("MockingTests::RecursiveGlobalDetouring", "[mocking]")
{
    {
        witest::detoured_global_function<&LocalAddFunction> detour;
        REQUIRE_SUCCEEDED(detour.reset([](int lhs, int rhs) {
            return lhs + rhs + LocalAddFunction(lhs * 2, rhs * 2);
        }));

        {
            witest::detoured_global_function<&LocalAddFunction> detour2;
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

TEST_CASE("MockingTests::ThreadDetourMoving", "[mocking]")
{
    witest::detoured_thread_function<&LocalAddFunction> outer;
    {
        witest::detoured_thread_function<&LocalAddFunction> middle;
        {
            witest::detoured_thread_function<&LocalAddFunction> inner;
            REQUIRE_SUCCEEDED(inner.reset([](int lhs, int rhs) {
                return lhs * rhs;
            }));
            REQUIRE(LocalAddFunction(2, 3) == 6);
            middle = std::move(inner);
        }
        REQUIRE(LocalAddFunction(2, 3) == 6);
        outer = std::move(middle);
    }
    REQUIRE(LocalAddFunction(2, 3) == 6);

    {
        witest::detoured_thread_function other(std::move(outer));
        REQUIRE(LocalAddFunction(2, 3) == 6);
    }

    REQUIRE(LocalAddFunction(2, 3) == 5); // Reverted back by now
}

TEST_CASE("MockingTests::ThreadDetourSwap", "[mocking]")
{
    {
        witest::detoured_thread_function<&LocalAddFunction> outer;
        REQUIRE_SUCCEEDED(outer.reset([](int lhs, int rhs) {
            return lhs * rhs;
        }));
        {
            witest::detoured_thread_function<&LocalAddFunction> inner;
            REQUIRE_SUCCEEDED(inner.reset([](int lhs, int rhs) {
                return 2 * LocalAddFunction(lhs, rhs);
            }));
            REQUIRE(LocalAddFunction(2, 3) == 12); // 2 * (2 * 3)
            inner.swap(outer);
            REQUIRE(LocalAddFunction(2, 3) == 12); // Order of evaluation should stay the same
            outer.swap(inner);                     // Swap the other way around
            REQUIRE(LocalAddFunction(2, 3) == 12); // Still the same...
            outer.swap(inner);                     // So that inner's lambda is copied into 'outer' when 'inner' goes out of scope
        }
        REQUIRE(LocalAddFunction(2, 3) == 10); // 2 * (2 + 3)
    }

    REQUIRE(LocalAddFunction(2, 3) == 5); // Reverted back by now
}

TEST_CASE("MockingTests::ThreadDetourDestructOutOfOrder", "[mocking]")
{
    {
        witest::detoured_thread_function<&LocalAddFunction> delayed;
        {
            // Under "normal" circumstances, registration behaves like a stack and we'll always remove from the head of the list.
            // Here we force the first registration to fall out of scope to test handling removal of the non-head element.
            witest::detoured_thread_function<&LocalAddFunction> first;
            REQUIRE_SUCCEEDED(first.reset([](int lhs, int rhs) {
                return lhs * rhs;
            }));
            REQUIRE_SUCCEEDED(delayed.reset([](int lhs, int rhs) {
                return 2 * (lhs + rhs);
            }));
            REQUIRE(LocalAddFunction(2, 3) == 10); // Should execute 'delayed'
        }

        REQUIRE(LocalAddFunction(2, 3) == 10); // Should still execute 'delayed'
    }

    REQUIRE(LocalAddFunction(2, 3) == 5); // Reverted back by now
}

TEST_CASE("MockingTests::GlobalDetourDestructOutOfOrder", "[mocking]")
{
    {
        witest::detoured_global_function<&LocalAddFunction> delayed;
        {
            // Under "normal" circumstances, registration behaves like a stack and we'll always remove from the head of the list.
            // Here we force the first registration to fall out of scope to test handling removal of the non-head element.
            witest::detoured_global_function<&LocalAddFunction> first;
            REQUIRE_SUCCEEDED(first.reset([](int lhs, int rhs) {
                return lhs * rhs;
            }));
            REQUIRE_SUCCEEDED(delayed.reset([](int lhs, int rhs) {
                return 2 * (lhs + rhs);
            }));
            REQUIRE(LocalAddFunction(2, 3) == 10); // Should execute 'delayed'
        }

        REQUIRE(LocalAddFunction(2, 3) == 10); // Should still execute 'delayed'
    }

    REQUIRE(LocalAddFunction(2, 3) == 5); // Reverted back by now
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

TEST_CASE("MockingTests::GlobalDetourMultithreaded", "[mocking]")
{
    witest::detoured_global_function<&LocalAddFunction> detour([](int lhs, int rhs) {
        return lhs * rhs;
    });

    int otherThreadResult = 0;
    std::thread thread([&] {
        otherThreadResult = LocalAddFunction(2, 3);
    });
    thread.join();
    REQUIRE(otherThreadResult == 6);
}

TEST_CASE("MockingTests::GlobalDetourDestructorRace", "[mocking]")
{
    wil::unique_event detourRunningEvent(wil::EventOptions::None);
    wil::unique_event nonDetourContinueEvent(wil::EventOptions::None);
    wil::unique_event nonDetourCompleteEvent(wil::EventOptions::None);
    witest::detoured_thread_function<&::SleepConditionVariableSRW> cvWaitDetour(
        [&](PCONDITION_VARIABLE cv, PSRWLOCK lock, DWORD dwMilliseconds, ULONG flags) {
            // This should be called during the call to 'reset' since there's an "active" call
            nonDetourContinueEvent.SetEvent(); // Kick off a non-detoured call
            return ::SleepConditionVariableSRW(cv, lock, dwMilliseconds, flags);
        });

    witest::detoured_global_function<&LocalAddFunction> detour([&](int lhs, int rhs) {
        detourRunningEvent.SetEvent();
        nonDetourCompleteEvent.wait(); // Wait until the non-detoured call is complete (implies we're in 'reset')
        return lhs * rhs;
    });

    int detouredResult = 0;
    std::thread detouredThread([&] {
        detouredResult = LocalAddFunction(2, 3);
    });

    int nonDetouredResult = 0;
    std::thread nonDetouredThread([&] {
        nonDetourContinueEvent.wait(); // Wait until 'reset' is called
        nonDetouredResult = LocalAddFunction(2, 3);
        nonDetourCompleteEvent.SetEvent(); // Let the original call complete, which allows 'reset' to complete
    });

    detourRunningEvent.wait(); // Wait for 'detouredThread' to kick off & invoke the detoured function
    detour.reset();            // Kick off everything to continue

    // By the time 'reset' completes, all calls should also have completed, hence the check before the calls to 'join' are fine
    REQUIRE(detouredResult == 6);
    REQUIRE(nonDetouredResult == 5);

    detouredThread.join();
    nonDetouredThread.join();
}
#endif
