// This file aims to test <wil/coroutine.h> with the COM support disabled.  However, COM
// headers are included by <windows.h> and an include of <windows.h> is unavoidable in
// this project.  The best we can do is to define WIN32_LEAN_AND_MEAN to prevent windows.h
// from including <ole2.h> (which in turn includes <combasepi.h>).
//
// We also avoid including "common.h" from the current directory because it pulls in COM too.
#include "pch.h"

#define WIN32_LEAN_AND_MEAN

#if defined(__cpp_impl_coroutine) || defined(__cpp_coroutines) || defined(_RESUMABLE_FUNCTIONS_SUPPORTED)
#include <wil/coroutine.h>
#include <thread>
#endif

#include "catch.hpp"

// We are intentionally not including any COM headers so that wil::task<> and
// wil::com_task<> do not light up full COM support.
#ifdef _COMBASEAPI_H_
#error "COM headers are included in a file that is testing non-COM support."
#endif

#if (!defined(__clang__) && defined(__cpp_impl_coroutine) && defined(__cpp_lib_coroutine) && (__cpp_lib_coroutine >= 201902L)) || \
    defined(_RESUMABLE_FUNCTIONS_SUPPORTED)

namespace
{
wil::task<void> void_task(std::shared_ptr<int> value)
{
    ++*value;
    co_return;
}

struct resume_new_cpp_thread
{
    bool await_ready() noexcept
    {
        return false;
    }
    template <typename Handle>
    void await_suspend(Handle handle) noexcept
    {
        std::thread([handle] {
            handle();
        }).detach();
    }
    void await_resume()
    {
    }
};

} // namespace

TEST_CASE("CppWinRTTests::SimpleNoCOMTaskTest", "[cppwinrt]")
{
    std::thread([] {
        auto value = std::make_shared<int>(0);
        void_task(value).get();
        REQUIRE(*value == 1);
    }).join();
}

TEST_CASE("CoroutineTests::WithWatcherBasic", "[coroutine]")
{
    // Test that wil::with_watcher wraps an awaitable and calls suspend()/resume()
    // on the watcher object across co_await.
    struct mock_watcher
    {
        int suspend_count = 0;
        int resume_count = 0;
        bool suspend() noexcept
        {
            ++suspend_count;
            return true;
        }
        void resume() noexcept
        {
            ++resume_count;
        }
    };

    auto test = [](mock_watcher& watcher) -> wil::task<void> {
        co_await wil::with_watcher(watcher, resume_new_cpp_thread{});
    };

    std::thread([&] {
        mock_watcher watcher;
        std::move(test(watcher)).get();
        REQUIRE(watcher.suspend_count == 1);
        REQUIRE(watcher.resume_count == 1);
    }).join();
}

TEST_CASE("CoroutineTests::WithWatcherSuspendReturnsFalse", "[coroutine]")
{
    // When suspend() returns false, resume() should not be called.
    struct mock_watcher
    {
        int suspend_count = 0;
        int resume_count = 0;
        bool suspend() noexcept
        {
            ++suspend_count;
            return false;
        }
        void resume() noexcept
        {
            ++resume_count;
        }
    };

    auto test = [](mock_watcher& watcher) -> wil::task<void> {
        co_await wil::with_watcher(watcher, resume_new_cpp_thread{});
    };

    std::thread([&] {
        mock_watcher watcher;
        std::move(test(watcher)).get();
        REQUIRE(watcher.suspend_count == 1);
        REQUIRE(watcher.resume_count == 0);
    }).join();
}

#endif // coroutines
