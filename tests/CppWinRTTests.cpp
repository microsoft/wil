
#include <wil/cppwinrt.h>
#include <winrt/Windows.Foundation.h>
#include <wil/cppwinrt_helpers.h>
#include <winrt/Windows.System.h>
#include <wil/cppwinrt_helpers.h> // Verify can include a second time to unlock more features

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

TEST_CASE("CppWinRTTests::ModuleReference", "[cppwinrt]")
{
    auto peek_module_ref_count = []()
    {
        ++winrt::get_module_lock();
        return --winrt::get_module_lock();
    };

    auto initial = peek_module_ref_count();

    // Basic test: Construct and destruct.
    {
        auto module_ref = wil::winrt_module_reference();
        REQUIRE(peek_module_ref_count() == initial + 1);
    }
    REQUIRE(peek_module_ref_count() == initial);

    // Fancy test: Copy object with embedded reference.
    {
        struct object_with_ref
        {
            wil::winrt_module_reference ref;
        };
        object_with_ref o1;
        REQUIRE(peek_module_ref_count() == initial + 1);
        auto o2 = o1;
        REQUIRE(peek_module_ref_count() == initial + 2);
        o1 = o2;
        REQUIRE(peek_module_ref_count() == initial + 2);
        o2 = std::move(o1);
        REQUIRE(peek_module_ref_count() == initial + 2);
    }
    REQUIRE(peek_module_ref_count() == initial);
}

#if (defined(__cpp_lib_coroutine) && (__cpp_lib_coroutine >= 201902L)) || defined(_RESUMABLE_FUNCTIONS_SUPPORTED)

// Define our own custom dispatcher that we can force it to behave in certain ways.
// wil::resume_foreground supports any dispatcher that has a dispatcher_traits.

namespace test
{
    enum class TestDispatcherPriority
    {
        Normal = 0,
        Weird = 1,
    };

    using TestDispatcherHandler = winrt::delegate<>;

    enum class TestDispatcherMode
    {
        Dispatch,
        RaceDispatch,
        Orphan,
        Fail,
    };

    struct TestDispatcher
    {
        TestDispatcher() = default;
        TestDispatcher(TestDispatcher const&) = delete;

        TestDispatcherMode mode = TestDispatcherMode::Dispatch;
        TestDispatcherPriority expected_priority = TestDispatcherPriority::Normal;

        void TryEnqueue(TestDispatcherPriority priority, TestDispatcherHandler const& handler) const
        {
            REQUIRE(priority == expected_priority);

            if (mode == TestDispatcherMode::Fail)
            {
                throw winrt::hresult_not_implemented();
            }

            if (mode == TestDispatcherMode::RaceDispatch)
            {
                handler();
                return;
            }

            auto background = [](auto mode, auto handler) ->winrt::fire_and_forget
            {
                co_await winrt::resume_background();
                if (mode == TestDispatcherMode::Dispatch)
                {
                    handler();
                }
            }(mode, handler);
        }
    };
}

namespace wil::details
{
    template<>
    struct dispatcher_traits<test::TestDispatcher>
    {
        using Priority = test::TestDispatcherPriority;
        using Handler = test::TestDispatcherHandler;
        using Scheduler = dispatcher_TryEnqueue;
    };
}

TEST_CASE("CppWinRTTests::ResumeForegroundTests", "[cppwinrt]")
{
    // Verify that the DispatcherQueue version has been unlocked.
    using Verify = decltype(wil::resume_foreground(winrt::Windows::System::DispatcherQueue{ nullptr }));

    []() -> winrt::Windows::Foundation::IAsyncAction
    {
        test::TestDispatcher dispatcher;

        // Normal case: Resumes on new thread.
        dispatcher.mode = test::TestDispatcherMode::Dispatch;
        co_await wil::resume_foreground(dispatcher);

        // Race case: Resumes before TryEnqueue returns.
        dispatcher.mode = test::TestDispatcherMode::RaceDispatch;
        co_await wil::resume_foreground(dispatcher);

        // Orphan case: Never resumes, detected when handler is destructed without ever being invoked.
        dispatcher.mode = test::TestDispatcherMode::Orphan;
        bool seen = false;
        try
        {
            co_await wil::resume_foreground(dispatcher);
        }
        catch (winrt::hresult_error const& e)
        {
            seen = e.code() == HRESULT_FROM_WIN32(HRESULT_FROM_WIN32(ERROR_NO_TASK_QUEUE));
        }
        REQUIRE(seen);

        // Fail case: Can't even schedule the resumption.
        dispatcher.mode = test::TestDispatcherMode::Fail;
        seen = false;
        try
        {
            co_await wil::resume_foreground(dispatcher);
        }
        catch (winrt::hresult_not_implemented const&)
        {
            seen = true;
        }
        REQUIRE(seen);

        // Custom priority.
        dispatcher.mode = test::TestDispatcherMode::Dispatch;
        dispatcher.expected_priority = test::TestDispatcherPriority::Weird;
        co_await wil::resume_foreground(dispatcher, test::TestDispatcherPriority::Weird);
    }().get();
}
#endif // coroutines
