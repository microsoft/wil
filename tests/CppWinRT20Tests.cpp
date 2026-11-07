
// Prior to C++/WinRT 2.0 this would cause issues since we're not including wil/cppwinrt.h in this translation unit.
// However, since we're going to link into the same executable as 'CppWinRTTests.cpp', the 'winrt_to_hresult_handler'
// global function pointer should be set, so these should all run successfully
#include "pch.h"

#include <inspectable.h> // Must be included before base.h

#include <wil/chrono.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <wil/chrono.h>
#include <wil/cppwinrt_helpers.h>
#include <wil/resource.h>
#include <wil/result.h>

#include <format>
#include <limits>

#include "common.h"

using namespace std::chrono_literals;

using floating_milliseconds = std::chrono::duration<double, std::milli>;
using third_seconds = std::chrono::duration<std::int64_t, std::ratio<1, 3>>;
using unsigned_milliseconds = std::chrono::duration<std::uint64_t, std::milli>;

template <typename Duration, typename = void>
struct can_try_to_relative_file_time : std::false_type
{
};

template <typename Duration>
struct can_try_to_relative_file_time<Duration, std::void_t<decltype(wil::try_to_relative_file_time(std::declval<Duration>(), static_cast<FILETIME*>(nullptr)))>>
    : std::true_type
{
};

static_assert(can_try_to_relative_file_time<std::chrono::nanoseconds>::value);
static_assert(!can_try_to_relative_file_time<floating_milliseconds>::value);
static_assert(!can_try_to_relative_file_time<third_seconds>::value);
static_assert(!can_try_to_relative_file_time<unsigned_milliseconds>::value);

static void CALLBACK SetCppWinRTEventTimerCallback(PTP_CALLBACK_INSTANCE, void* context, PTP_TIMER)
{
    ::SetEvent(static_cast<HANDLE>(context));
}

TEST_CASE("CppWinRTTests::CppWinRT20Test", "[cppwinrt]")
{
    auto test = [](HRESULT hr) {
        try
        {
            THROW_HR(hr);
        }
        catch (...)
        {
            REQUIRE(hr == winrt::to_hresult());
        }
    };

    test(E_OUTOFMEMORY);
    test(E_INVALIDARG);
    test(E_UNEXPECTED);
}

TEST_CASE("CppWinRTTests::ChronoInterop", "[cppwinrt][chrono]")
{
    constexpr std::int64_t c_unixEpochOffsetInFileTimeTicks = 116444736000000000LL;

    const auto time = winrt::clock::from_sys(std::chrono::system_clock::time_point{});
    REQUIRE(time.time_since_epoch().count() == c_unixEpochOffsetInFileTimeTicks);
    REQUIRE(winrt::clock::to_sys(time) == std::chrono::system_clock::time_point{});
    REQUIRE(std::format("{:%Y-%m-%d %H:%M:%S}", winrt::clock::to_sys(time)) == "1970-01-01 00:00:00.0000000");

    FILETIME fileTime{};
    REQUIRE_SUCCEEDED(wil::details::try_to_file_time(time, &fileTime));
    REQUIRE(winrt::file_time{fileTime}.value == static_cast<std::uint64_t>(c_unixEpochOffsetInFileTimeTicks));
    REQUIRE_SUCCEEDED(wil::details::try_to_file_time(std::chrono::system_clock::time_point{}, &fileTime));
    REQUIRE(winrt::file_time{fileTime}.value == static_cast<std::uint64_t>(c_unixEpochOffsetInFileTimeTicks));
}

TEST_CASE("CppWinRTTests::RelativeFileTimeAndTimer", "[cppwinrt][chrono]")
{
    FILETIME value{};

    REQUIRE_SUCCEEDED(wil::try_to_relative_file_time(0ns, &value));
    REQUIRE(winrt::file_time{value}.value == 0);

    REQUIRE_SUCCEEDED(wil::try_to_relative_file_time(1ns, &value));
    REQUIRE(winrt::file_time{value}.value == static_cast<std::uint64_t>(-1LL));

    REQUIRE_SUCCEEDED(wil::try_to_relative_file_time(1ms, &value));
    REQUIRE(winrt::file_time{value}.value == static_cast<std::uint64_t>(-10000LL));

    // INT64_MAX 100-nanosecond ticks is the largest supported relative interval. Its negative two's-complement
    // FILETIME representation is 0x8000000000000001.
    REQUIRE_SUCCEEDED(wil::try_to_relative_file_time(winrt::clock::duration{(std::numeric_limits<std::int64_t>::max)()}, &value));
    REQUIRE(winrt::file_time{value}.value == 0x8000000000000001ULL);

    REQUIRE(wil::try_to_relative_file_time(-1ns, &value) == E_INVALIDARG);

    wil::unique_event_nothrow event;
    REQUIRE_SUCCEEDED(event.create(wil::EventOptions::ManualReset));
    wil::unique_threadpool_timer timer{::CreateThreadpoolTimer(SetCppWinRTEventTimerCallback, event.get(), nullptr)};
    REQUIRE(timer);

    REQUIRE_SUCCEEDED(wil::set_relative_threadpool_timer_nothrow(timer.get(), 1ms));
    REQUIRE(event.wait(5s));

    REQUIRE(wil::set_relative_threadpool_timer_nothrow(timer.get(), -1ms) == E_INVALIDARG);

    // SetThreadpoolTimer treats any nonzero DWORD period/window as a value; INFINITE is not reserved for these parameters.
    REQUIRE_SUCCEEDED(
        wil::set_relative_threadpool_timer_nothrow(
            timer.get(),
            1h,
            std::chrono::milliseconds{static_cast<std::int64_t>((std::numeric_limits<DWORD>::max)())},
            std::chrono::milliseconds{static_cast<std::int64_t>((std::numeric_limits<DWORD>::max)())}));

    event.ResetEvent();
    REQUIRE_SUCCEEDED(wil::set_threadpool_timer_nothrow(timer.get(), std::chrono::system_clock::now() + 1ms));
    REQUIRE(event.wait(5s));
}
