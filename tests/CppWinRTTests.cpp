
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
// so that we aren't using com_task to validate itself.

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

    wil::com_task<void> void_com_task(std::shared_ptr<int> value, HANDLE h)
    {
        if (h) co_await winrt::resume_on_signal(h);
        ++* value;
        co_return;
    }

    // Return a reference to the wrapped integer.
    wil::com_task<int&> intref_com_task(std::shared_ptr<int> value, HANDLE h)
    {
        co_await void_com_task(value, h);
        co_return *value;
    }

    // Return a move-only type.
    wil::com_task<wil::unique_cotaskmem_string> string_com_task(HANDLE h)
    {
        if (h) co_await winrt::resume_on_signal(h);
        co_return wil::make_cotaskmem_string(L"Hello");
    }

    // Return a move-only type with agile resumption.
    wil::task<wil::unique_cotaskmem_string> string_task(HANDLE h)
    {
        if (h) co_await winrt::resume_on_signal(h);
        co_return wil::make_cotaskmem_string(L"Hello");
    }

    wil::com_task<void> exception_com_task(HANDLE h)
    {
        if (h) co_await winrt::resume_on_signal(h);
        throw 42; // throw some random exception
    }

    wil::com_task<void> test_sta_task(HANDLE e)
    {
        auto on_ui_thread = [originalThread = GetCurrentThreadId()]
        { return originalThread == GetCurrentThreadId(); };

        // Signal the incoming event handle when the coroutine has completed.
        auto complete = wil::SetEvent_scope_exit(e);

        // Create our own event handle to force race conditions.
        auto sync = wil::unique_event(wil::EventOptions::ManualReset);

        // Remember original thread so we can return to it at the start of each test (if desired).
        winrt::apartment_context context;

        // Basic test of com_task, ensuring that we return to the UI thread.
        co_await context; // start on UI thread
        auto value = std::make_shared<int>(1);
        signal_later(sync.get()); // prevent void_com_task from completing before we call await_ready
        co_await void_com_task(value, sync.get());
        REQUIRE(*value == 2);
        REQUIRE(on_ui_thread());

        // Fancier version that produces a reference (which PPL and C++/WinRT don't support).
        co_await context; // start on UI thread
        signal_later(sync.get()); // prevent intref_com_task from completing before we call await_ready
        int& valueRef = co_await intref_com_task(value, sync.get());
        REQUIRE(wistd::addressof(valueRef) == wistd::addressof(*value));
        REQUIRE(*value == 3);
        REQUIRE(on_ui_thread());

        // Test forced agility via task conversion.
        co_await context; // start on UI thread
        signal_later(sync.get()); // prevent void_com_task from completing before we call await_ready
        co_await wil::task(void_com_task(value, sync.get()));
        REQUIRE(*value == 4);
        REQUIRE(!on_ui_thread());

        // Test that co_await of a com_task from a threadpool thread stays on the threadpool.
        // Also test move-only type.
        co_await winrt::resume_background(); // start on non-UI thread
        signal_later(sync.get()); // prevent string_com_task from completing before we call await_ready
        auto str = co_await string_com_task(sync.get());
        REQUIRE(wcscmp(str.get(), L"Hello") == 0);
        REQUIRE(!on_ui_thread());

        // Test forced agility via resume_any_thread.
        co_await context; // start on UI thread
        signal_later(sync.get()); // prevent string_com_task from completing before we call await_ready
        str = co_await string_com_task(sync.get()).resume_any_thread();
        REQUIRE(wcscmp(str.get(), L"Hello") == 0);
        REQUIRE(!on_ui_thread());

        // Test exceptions.
        co_await context; // start on UI thread
        signal_later(sync.get()); // prevent exception_com_task from completing before we call await_ready
        REQUIRE_THROWS_AS(co_await exception_com_task(sync.get()), int);
        REQUIRE(on_ui_thread());

        // Test forced apartment awareness via task conversion.
        signal_later(sync.get()); // prevent string_task from completing before we call await_ready
        str = co_await wil::com_task(string_task(sync.get()));
        REQUIRE(wcscmp(str.get(), L"Hello") == 0);
        REQUIRE(on_ui_thread());

        // Test forced apartment awareness via resume_same_apartment.
        co_await context; // start on UI thread
        signal_later(sync.get()); // prevent string_task from completing before we call await_ready
        str = co_await string_task(sync.get()).resume_same_apartment();
        REQUIRE(wcscmp(str.get(), L"Hello") == 0);
        REQUIRE(on_ui_thread());

        // Test agile task
        co_await context; // start on UI thread
        signal_later(sync.get()); // prevent string_task from completing before we call await_ready
        str = co_await string_task(sync.get());
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
            void_com_task(value, nullptr).get();
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