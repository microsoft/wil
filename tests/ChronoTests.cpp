#include "pch.h"

#include <wil/chrono.h>
#include <wil/resource.h>

#include "common.h"

using namespace std::chrono_literals;

using floating_milliseconds = std::chrono::duration<double, std::milli>;
using third_seconds = std::chrono::duration<std::int64_t, std::ratio<1, 3>>;
using unsigned_milliseconds = std::chrono::duration<std::uint64_t, std::milli>;

template <typename Duration, typename = void>
struct can_try_to_dword_ms : std::false_type
{
};

template <typename Duration>
struct can_try_to_dword_ms<Duration, std::void_t<decltype(wil::try_to_dword_ms(std::declval<Duration>(), static_cast<DWORD*>(nullptr)))>>
    : std::true_type
{
};

template <typename Duration, typename = void>
struct can_to_float_ms : std::false_type
{
};

template <typename Duration>
struct can_to_float_ms<Duration, std::void_t<decltype(wil::to_float_ms(std::declval<Duration>()))>> : std::true_type
{
};

static_assert(wil::to_float_ms(1500us) == 1.5f);
static_assert(can_try_to_dword_ms<std::chrono::nanoseconds>::value);
static_assert(can_to_float_ms<std::chrono::seconds>::value);
static_assert(!can_try_to_dword_ms<floating_milliseconds>::value);
static_assert(!can_to_float_ms<floating_milliseconds>::value);
static_assert(!can_try_to_dword_ms<third_seconds>::value);
static_assert(!can_to_float_ms<third_seconds>::value);
static_assert(!can_try_to_dword_ms<unsigned_milliseconds>::value);
static_assert(!can_to_float_ms<unsigned_milliseconds>::value);

TEST_CASE("ChronoTests::DwordMilliseconds", "[chrono]")
{
    DWORD value{};

    REQUIRE_SUCCEEDED(wil::try_to_dword_ms(0ns, &value));
    REQUIRE(value == 0);
    REQUIRE_SUCCEEDED(wil::try_to_dword_ms(1ns, &value));
    REQUIRE(value == 1);
    REQUIRE_SUCCEEDED(wil::try_to_dword_ms(1001us, &value));
    REQUIRE(value == 2);
    REQUIRE_SUCCEEDED(wil::try_to_dword_ms(2s, &value));
    REQUIRE(value == 2000);
    REQUIRE_SUCCEEDED(wil::try_to_dword_ms(std::chrono::milliseconds{INFINITE - 1}, &value));
    REQUIRE(value == INFINITE - 1);

    REQUIRE(wil::try_to_dword_ms(-1ms, &value) == E_INVALIDARG);
    REQUIRE(
        wil::try_to_dword_ms(std::chrono::milliseconds{static_cast<std::int64_t>(INFINITE)}, &value) ==
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
}

TEST_CASE("ChronoTests::FloatMilliseconds", "[chrono]")
{
    REQUIRE(wil::to_float_ms(1ns) == Catch::Approx(0.000001f));
    REQUIRE(wil::to_float_ms(1500us) == Catch::Approx(1.5f));
    REQUIRE(wil::to_float_ms(-2s) == Catch::Approx(-2000.0f));
}

TEST_CASE("ChronoTests::ChronoEventWait", "[chrono]")
{
    wil::unique_event_nothrow event;
    REQUIRE_SUCCEEDED(event.create());
    REQUIRE(!event.wait(1ms));
    event.SetEvent();
    REQUIRE(event.wait(1s));
}
