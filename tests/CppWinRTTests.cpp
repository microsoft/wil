
#include <wil/cppwinrt.h>
#if defined(_RESUMABLE_FUNCTIONS_SUPPORTED) || defined(__cpp_lib_coroutine)
#include <wil/coroutine.h>
#include <winrt/Windows.Foundation.h> // use C++/WinRT coroutines to test wil coroutines
#include <thread>
#endif

#include "catch.hpp"

// HRESULT values that C++/WinRT throws as something other than winrt::hresult_error - e.g. a type derived from
// winrt::hresult_error, std::*, etc.
static const HRESULT cppwinrt_mapped_hresults[] =
{
    E_ACCESSDENIED,
    RPC_E_WRONG_THREAD,
    E_NOTIMPL,
    E_INVALIDARG,
    E_BOUNDS,
    E_NOINTERFACE,
    CLASS_E_CLASSNOTAVAILABLE,
    E_CHANGED_STATE,
    E_ILLEGAL_METHOD_CALL,
    E_ILLEGAL_STATE_CHANGE,
    E_ILLEGAL_DELEGATE_ASSIGNMENT,
    HRESULT_FROM_WIN32(ERROR_CANCELLED),
    E_OUTOFMEMORY,
};

TEST_CASE("CppWinRTTests::WilToCppWinRTExceptionTranslationTest", "[cppwinrt]")
{
    auto test = [](HRESULT hr)
    {
        try
        {
            THROW_HR(hr);
        }
        catch (...)
        {
            REQUIRE(hr == winrt::to_hresult());
        }
    };

    for (auto hr : cppwinrt_mapped_hresults)
    {
        test(hr);
    }

    // A non-mapped HRESULT
    test(E_UNEXPECTED);
}

TEST_CASE("CppWinRTTests::CppWinRTToWilExceptionTranslationTest", "[cppwinrt]")
{
    auto test = [](HRESULT hr)
    {
        try
        {
            winrt::check_hresult(hr);
        }
        catch (...)
        {
            REQUIRE(hr == wil::ResultFromCaughtException());
        }
    };

    for (auto hr : cppwinrt_mapped_hresults)
    {
        test(hr);
    }

    // A non-mapped HRESULT
    test(E_UNEXPECTED);
}

TEST_CASE("CppWinRTTests::ResultFromExceptionDebugTest", "[cppwinrt]")
{
    auto test = [](HRESULT hr, wil::SupportedExceptions supportedExceptions)
    {
        auto result = wil::ResultFromExceptionDebug(WI_DIAGNOSTICS_INFO, supportedExceptions, [&]()
        {
            winrt::check_hresult(hr);
        });
        REQUIRE(hr == result);
    };

    for (auto hr : cppwinrt_mapped_hresults)
    {
        test(hr, wil::SupportedExceptions::Known);
        test(hr, wil::SupportedExceptions::All);
    }

    // A non-mapped HRESULT
    test(E_UNEXPECTED, wil::SupportedExceptions::Known);
    test(E_UNEXPECTED, wil::SupportedExceptions::All);

    // Uncomment any of the following to validate SEH failfast
    //test(E_UNEXPECTED, wil::SupportedExceptions::None);
    //test(E_ACCESSDENIED, wil::SupportedExceptions::Thrown);
    //test(E_INVALIDARG, wil::SupportedExceptions::ThrownOrAlloc);
}

TEST_CASE("CppWinRTTests::CppWinRTConsistencyTest", "[cppwinrt]")
{
    // Since setting 'winrt_to_hresult_handler' opts us into _all_ C++/WinRT exception translation handling, we need to
    // make sure that we preserve behavior, at least with 'check_hresult', especially when C++/WinRT maps a particular
    // HRESULT value to a different exception type
    auto test = [](HRESULT hr)
    {
        try
        {
            winrt::check_hresult(hr);
        }
        catch (...)
        {
            REQUIRE(hr == winrt::to_hresult());
        }
    };

    for (auto hr : cppwinrt_mapped_hresults)
    {
        test(hr);
    }

    // A non-mapped HRESULT
    test(E_UNEXPECTED);

    // C++/WinRT also maps a few std::* exceptions to various HRESULTs. We should preserve this behavior
    try
    {
        throw std::out_of_range("oopsie");
    }
    catch (...)
    {
        REQUIRE(winrt::to_hresult() == E_BOUNDS);
    }

    try
    {
        throw std::invalid_argument("daisy");
    }
    catch (...)
    {
        REQUIRE(winrt::to_hresult() == E_INVALIDARG);
    }

    // NOTE: C++/WinRT maps other 'std::exception' derived exceptions to E_FAIL, however we preserve the WIL behavior
    // that such exceptions become HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION)
}

#if defined(_RESUMABLE_FUNCTIONS_SUPPORTED) || defined(__cpp_lib_coroutine)

// Note that we use C++/WinRT's coroutines in the test framework,
// so that we aren't using simple_task to validate itself.

namespace
{
    // Helper coroutine that lets us pause another coroutine
    // until after we start co_await'ing for it.
    winrt::fire_and_forget signal_later(HANDLE h)
    {
        winrt::apartment_context context;

        ResetEvent(h);
        co_await winrt::resume_background();

        // The return to the STA thread occurs after
        // the STA thread's current coroutine suspends.
        co_await context;

        SetEvent(h);
    }

    wil::simple_task<void> void_task(std::shared_ptr<int> value, HANDLE h)
    {
        if (h) co_await winrt::resume_on_signal(h);
        ++* value;
        co_return;
    }

    // Return a reference to the wrapped integer.
    wil::simple_task<int&> intref_task(std::shared_ptr<int> value, HANDLE h)
    {
        co_await void_task(value, h);
        co_return *value;
    }

    // Return a move-only type.
    wil::simple_task<wil::unique_cotaskmem_string> string_task(HANDLE h)
    {
        if (h) co_await winrt::resume_on_signal(h);
        co_return wil::make_cotaskmem_string(L"Hello");
    }

    // Return a move-only type with agile resumption.
    wil::simple_agile_task<wil::unique_cotaskmem_string> string_agile_task(HANDLE h)
    {
        if (h) co_await winrt::resume_on_signal(h);
        co_return wil::make_cotaskmem_string(L"Hello");
    }

    wil::simple_task<void> exception_task(HANDLE h)
    {
        if (h) co_await winrt::resume_on_signal(h);
        THROW_HR(E_ABORT);
    }

    wil::simple_task<void> test_sta_task(HANDLE e)
    {
        auto on_ui_thread = [originalThread = GetCurrentThreadId()]
        { return originalThread == GetCurrentThreadId(); };

        // Use the incoming event handle to know when the coroutine has completed.
        auto complete = wil::SetEvent_scope_exit(e);

        // Create our own event handle to force race conditions.
        auto sync = wil::unique_event(wil::EventOptions::ManualReset);

        // Remember original thread so we can return to it.
        winrt::apartment_context context;

        // Basic test of simple_task, ensuring that we return to the UI thread.
        auto value = std::make_shared<int>(1);
        signal_later(sync.get()); // prevent void_task from completing before we call await_ready
        co_await void_task(value, sync.get());
        REQUIRE(*value == 2);
        REQUIRE(on_ui_thread());

        // Fancier version that produces a reference (which PPL and C++/WinRT don't support).
        signal_later(sync.get()); // prevent intref_task from completing before we call await_ready
        int& valueRef = co_await intref_task(value, sync.get());
        REQUIRE(wistd::addressof(valueRef) == wistd::addressof(*value));
        REQUIRE(*value == 3);
        REQUIRE(on_ui_thread());

        // Test forced agility.
        signal_later(sync.get()); // prevent void_task from completing before we call await_ready
        co_await wil::simple_agile_task(void_task(value, sync.get()));
        REQUIRE(*value == 4);
        REQUIRE(!on_ui_thread());

        // And then the next non-agile co_await stays on the threadpool.
        // Also test move-only type.
        signal_later(sync.get()); // prevent string_task from completing before we call await_ready
        auto str = co_await string_task(sync.get());
        REQUIRE(wcscmp(str.get(), L"Hello") == 0);
        REQUIRE(!on_ui_thread());

        // Back to the UI thread.
        co_await context;

        // Test exceptions.
        signal_later(sync.get()); // prevent exception_task from completing before we call await_ready
        REQUIRE_THROWS_AS(co_await exception_task(sync.get()), wil::ResultException);
        REQUIRE(on_ui_thread());

        // Test agile task
        signal_later(sync.get()); // prevent exception_task from completing before we call await_ready
        str = co_await string_agile_task(sync.get());
        REQUIRE(wcscmp(str.get(), L"Hello") == 0);
        REQUIRE(!on_ui_thread());
    }
}

TEST_CASE("CppWinRTTests::SimpleTaskTest", "[cppwinrt]")
{
    std::thread([]
        {
            // MTA tests
            wil::unique_mta_usage_cookie cookie;
            REQUIRE(CoIncrementMTAUsage(cookie.put()) == S_OK);
            auto value = std::make_shared<int>(0);
            void_task(value, nullptr).get();
            REQUIRE(*value == 1);
            // Keep MTA active while we run the STA tests.

            // STA tests
            auto init = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);

            auto done = wil::shared_event(wil::unique_event(wil::EventOptions::ManualReset));
            auto handle = done.get();
            auto task = test_sta_task(handle);
            DWORD waitResult;
            while ((waitResult = MsgWaitForMultipleObjects(1, &handle, false, INFINITE, QS_ALLEVENTS)) == WAIT_OBJECT_0 + 1)
            {
                MSG msg;
                while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
    ).join();
}
#endif