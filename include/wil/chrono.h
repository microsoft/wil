//*********************************************************
//
//    Copyright (c) Microsoft. All rights reserved.
//    This code is licensed under the MIT License.
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
//    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//    PARTICULAR PURPOSE AND NONINFRINGEMENT.
//
//*********************************************************
//! @file
//! Helpers for converting between C++ chrono types and Windows time representations.
#ifndef __WIL_CHRONO_INCLUDED
#define __WIL_CHRONO_INCLUDED

#include "result_macros.h"

#if WIL_USE_STL && !defined(WIL_NO_CHRONO) && !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE)

#include <chrono>
#include <cstdint>
#include <limits>
#include <ratio>
#include <type_traits>
#include <utility>

namespace wil
{
namespace details
{
    template <typename Duration, typename TargetPeriod>
    struct is_supported_duration_conversion : std::false_type
    {
    };

    template <typename Rep, typename Period, typename TargetPeriod>
    struct is_supported_duration_conversion<std::chrono::duration<Rep, Period>, TargetPeriod>
        : std::bool_constant<
              // Standard clocks and duration aliases use signed integral reps. Limiting reps to that common case
              // avoids separate unsigned and floating-point overflow rules.
              std::is_integral_v<Rep> && std::is_signed_v<Rep> && (sizeof(Rep) <= sizeof(std::int64_t)) &&
              // The reduced conversion may multiply or divide, but not both. Callers with unusual periods must
              // explicitly cast first instead of relying on a general multiply-divide implementation.
              ((std::ratio_divide<Period, TargetPeriod>::num == 1) || (std::ratio_divide<Period, TargetPeriod>::den == 1))>
    {
    };

    template <typename Duration, typename TargetPeriod>
    using duration_conversion_ratio_t = std::ratio_divide<typename Duration::period, TargetPeriod>;

    template <typename Duration, typename TargetPeriod>
    inline constexpr bool is_supported_duration_conversion_v = is_supported_duration_conversion<Duration, TargetPeriod>::value;

    template <typename Duration, typename TargetPeriod>
    using enable_if_supported_duration_conversion_t =
        std::enable_if_t<is_supported_duration_conversion_v<Duration, TargetPeriod>, int>;

    template <typename Duration, typename TargetPeriod>
    HRESULT try_nonnegative_duration_ceiling(Duration value, std::uint64_t maximum, std::uint64_t* result) WI_NOEXCEPT
    {
        static_assert(is_supported_duration_conversion_v<Duration, TargetPeriod>);
        using ratio = duration_conversion_ratio_t<Duration, TargetPeriod>;

        const auto count = static_cast<std::int64_t>(value.count());
        if (count < 0)
        {
            return E_INVALIDARG;
        }

        const auto unsignedCount = static_cast<std::uint64_t>(count);
        if constexpr (ratio::den == 1)
        {
            // A finer source period requires multiplication. Check the final bound before multiplying so the
            // intermediate value cannot overflow.
            if (unsignedCount > maximum / ratio::num)
            {
                return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
            }
            *result = unsignedCount * ratio::num;
        }
        else
        {
            // A coarser source period requires division. Win32 timeout and relative-timer APIs must not shorten a
            // positive duration, so any discarded fraction rounds upward.
            const auto converted = (unsignedCount / ratio::den) + ((unsignedCount % ratio::den) != 0);
            if (converted > maximum)
            {
                return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
            }
            *result = converted;
        }

        return S_OK;
    }

    template <typename Rep, typename Period>
    HRESULT try_to_dword_ms_with_maximum(std::chrono::duration<Rep, Period> value, DWORD maximum, DWORD* result) WI_NOEXCEPT
    {
        std::uint64_t converted{};
        RETURN_IF_FAILED((try_nonnegative_duration_ceiling<std::chrono::duration<Rep, Period>, std::milli>(value, maximum, &converted)));
        *result = static_cast<DWORD>(converted);
        return S_OK;
    }
} // namespace details

//! Converts an integral chrono duration to fractional milliseconds for measurement and telemetry. Unlike `to_dword_ms`, this
//! preserves negative values and fractions. Floating-point input durations and periods requiring both multiplication and division
//! are intentionally unsupported. Unsigned representations are also unsupported because standard clocks and duration aliases use
//! signed representations, and rejecting custom unsigned durations keeps checked conversions simple.
//! Explicitly cast unsupported values to a supported integral duration first.
//! For example, `wil::to_float_ms(end - start)` converts an elapsed duration to a floating-point telemetry field.
template <typename Rep, typename Period, details::enable_if_supported_duration_conversion_t<std::chrono::duration<Rep, Period>, std::milli> = 0>
constexpr float to_float_ms(std::chrono::duration<Rep, Period> value) WI_NOEXCEPT
{
    return std::chrono::duration<float, std::milli>{value}.count();
}

//! Converts a supported integral chrono duration to a finite Win32 millisecond timeout, rounding positive fractions upward.
template <typename Rep, typename Period, details::enable_if_supported_duration_conversion_t<std::chrono::duration<Rep, Period>, std::milli> = 0>
HRESULT try_to_dword_ms(std::chrono::duration<Rep, Period> value, DWORD* result) WI_NOEXCEPT
{
    return details::try_to_dword_ms_with_maximum(value, INFINITE - 1, result);
}

template <typename Rep, typename Period, details::enable_if_supported_duration_conversion_t<std::chrono::duration<Rep, Period>, std::milli> = 0>
DWORD to_dword_ms_failfast(std::chrono::duration<Rep, Period> value) WI_NOEXCEPT
{
    DWORD result{};
    FAIL_FAST_IF_FAILED(try_to_dword_ms(value, &result));
    return result;
}

#if defined(WIL_ENABLE_EXCEPTIONS)
template <typename Rep, typename Period, details::enable_if_supported_duration_conversion_t<std::chrono::duration<Rep, Period>, std::milli> = 0>
DWORD to_dword_ms(std::chrono::duration<Rep, Period> value)
{
    DWORD result{};
    THROW_IF_FAILED(try_to_dword_ms(value, &result));
    return result;
}
#endif

} // namespace wil

#endif // WIL_USE_STL && !defined(WIL_NO_CHRONO) && !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE)
#endif // __WIL_CHRONO_INCLUDED

// Keep C++/WinRT-dependent helpers outside the primary include guard. A caller may include this header for the generic
// duration helpers, include winrt/base.h later, and then include this header again to enable FILETIME and timer helpers.
#if WIL_USE_STL && !defined(WIL_NO_CHRONO) && !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE) && \
    defined(WINRT_BASE_H) && !defined(__WIL_CHRONO_WINRT_CLOCK)
#define __WIL_CHRONO_WINRT_CLOCK

namespace wil
{
//! Encodes a nonnegative duration as the signed relative FILETIME representation used by threadpool timers.
template <typename Rep, typename Period, details::enable_if_supported_duration_conversion_t<std::chrono::duration<Rep, Period>, winrt::clock::period> = 0>
HRESULT try_to_relative_file_time(std::chrono::duration<Rep, Period> value, FILETIME* result) WI_NOEXCEPT
{
    std::uint64_t roundedTicks{};
    RETURN_IF_FAILED((details::try_nonnegative_duration_ceiling<std::chrono::duration<Rep, Period>, winrt::clock::period>(
        value, static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()), &roundedTicks)));
    // SetThreadpoolTimer interprets negative 100-nanosecond counts as relative time. Form the two's-complement
    // representation explicitly; zero remains an immediate due time rather than a negative interval.
    const auto encoded = roundedTicks == 0 ? 0ULL : (~roundedTicks) + 1;
    *result = static_cast<FILETIME>(winrt::file_time{encoded});
    return S_OK;
}

namespace details
{
    inline HRESULT try_to_file_time(winrt::clock::time_point value, FILETIME* result) WI_NOEXCEPT
    {
        const auto ticks = value.time_since_epoch().count();
        if (ticks < 0)
        {
            return E_INVALIDARG;
        }

        *result = static_cast<FILETIME>(winrt::clock::to_file_time(value));
        return S_OK;
    }

    inline HRESULT try_to_file_time(std::chrono::system_clock::time_point value, FILETIME* result) WI_NOEXCEPT
    {
        static_assert(std::ratio_equal_v<std::chrono::system_clock::period, winrt::clock::period>, "system_clock must use the Windows FILETIME period");
        return try_to_file_time(winrt::clock::from_sys(value), result);
    }

    template <typename DueTime, typename PeriodDuration, typename WindowDuration>
    HRESULT set_threadpool_timer_nothrow_impl(PTP_TIMER timer, DueTime due, PeriodDuration period, WindowDuration window) WI_NOEXCEPT
    {
        if (timer == nullptr)
        {
            return E_INVALIDARG;
        }

        FILETIME dueTime{};
        DWORD periodMilliseconds{};
        DWORD windowMilliseconds{};
        RETURN_IF_FAILED(try_to_file_time(due, &dueTime));
        RETURN_IF_FAILED(try_to_dword_ms_with_maximum(period, (std::numeric_limits<DWORD>::max)(), &periodMilliseconds));
        RETURN_IF_FAILED(try_to_dword_ms_with_maximum(window, (std::numeric_limits<DWORD>::max)(), &windowMilliseconds));
        ::SetThreadpoolTimer(timer, &dueTime, periodMilliseconds, windowMilliseconds);
        return S_OK;
    }
} // namespace details

/** Schedules a threadpool timer relative to the current time.
Use the `_nothrow` form to return conversion failures, the `_failfast` form when invalid values are fatal, or the throwing form
when exceptions are enabled.
@code
RETURN_IF_FAILED(wil::set_relative_threadpool_timer_nothrow(timer.get(), std::chrono::milliseconds{250}));
wil::set_relative_threadpool_timer_failfast(timer.get(), std::chrono::milliseconds{250});
wil::set_relative_threadpool_timer(timer.get(), std::chrono::milliseconds{250});
@endcode
*/
template <
    typename DueDuration,
    typename PeriodDuration = std::chrono::milliseconds,
    typename WindowDuration = std::chrono::milliseconds,
    details::enable_if_supported_duration_conversion_t<DueDuration, winrt::clock::period> = 0,
    details::enable_if_supported_duration_conversion_t<PeriodDuration, std::milli> = 0,
    details::enable_if_supported_duration_conversion_t<WindowDuration, std::milli> = 0>
HRESULT set_relative_threadpool_timer_nothrow(
    PTP_TIMER timer, DueDuration due, PeriodDuration period = PeriodDuration::zero(), WindowDuration window = WindowDuration::zero()) WI_NOEXCEPT
{
    if (timer == nullptr)
    {
        return E_INVALIDARG;
    }

    FILETIME dueTime{};
    DWORD periodMilliseconds{};
    DWORD windowMilliseconds{};
    RETURN_IF_FAILED(try_to_relative_file_time(due, &dueTime));
    RETURN_IF_FAILED(details::try_to_dword_ms_with_maximum(period, (std::numeric_limits<DWORD>::max)(), &periodMilliseconds));
    RETURN_IF_FAILED(details::try_to_dword_ms_with_maximum(window, (std::numeric_limits<DWORD>::max)(), &windowMilliseconds));
    ::SetThreadpoolTimer(timer, &dueTime, periodMilliseconds, windowMilliseconds);
    return S_OK;
}

template <typename... Args>
void set_relative_threadpool_timer_failfast(Args&&... args) WI_NOEXCEPT
{
    FAIL_FAST_IF_FAILED(set_relative_threadpool_timer_nothrow(std::forward<Args>(args)...));
}

#if defined(WIL_ENABLE_EXCEPTIONS)
template <typename... Args>
void set_relative_threadpool_timer(Args&&... args)
{
    THROW_IF_FAILED(set_relative_threadpool_timer_nothrow(std::forward<Args>(args)...));
}
#endif

/** Schedules a threadpool timer for an absolute C++/WinRT or system clock time.
Use the `_nothrow` form to return conversion failures, the `_failfast` form when invalid values are fatal, or the throwing form
when exceptions are enabled.
@code
const auto due = std::chrono::system_clock::now() + std::chrono::seconds{1};
RETURN_IF_FAILED(wil::set_threadpool_timer_nothrow(timer.get(), due));
wil::set_threadpool_timer_failfast(timer.get(), due);
wil::set_threadpool_timer(timer.get(), due);
@endcode
*/
template <
    typename PeriodDuration = std::chrono::milliseconds,
    typename WindowDuration = std::chrono::milliseconds,
    details::enable_if_supported_duration_conversion_t<PeriodDuration, std::milli> = 0,
    details::enable_if_supported_duration_conversion_t<WindowDuration, std::milli> = 0>
HRESULT set_threadpool_timer_nothrow(
    PTP_TIMER timer,
    winrt::clock::time_point due,
    PeriodDuration period = PeriodDuration::zero(),
    WindowDuration window = WindowDuration::zero()) WI_NOEXCEPT
{
    return details::set_threadpool_timer_nothrow_impl(timer, due, period, window);
}

template <
    typename PeriodDuration = std::chrono::milliseconds,
    typename WindowDuration = std::chrono::milliseconds,
    details::enable_if_supported_duration_conversion_t<PeriodDuration, std::milli> = 0,
    details::enable_if_supported_duration_conversion_t<WindowDuration, std::milli> = 0>
HRESULT set_threadpool_timer_nothrow(
    PTP_TIMER timer,
    std::chrono::system_clock::time_point due,
    PeriodDuration period = PeriodDuration::zero(),
    WindowDuration window = WindowDuration::zero()) WI_NOEXCEPT
{
    return details::set_threadpool_timer_nothrow_impl(timer, due, period, window);
}

template <typename... Args>
void set_threadpool_timer_failfast(Args&&... args) WI_NOEXCEPT
{
    FAIL_FAST_IF_FAILED(set_threadpool_timer_nothrow(std::forward<Args>(args)...));
}

#if defined(WIL_ENABLE_EXCEPTIONS)
template <typename... Args>
void set_threadpool_timer(Args&&... args)
{
    THROW_IF_FAILED(set_threadpool_timer_nothrow(std::forward<Args>(args)...));
}
#endif
} // namespace wil

#endif // WINRT_BASE_H && !defined(__WIL_CHRONO_WINRT_CLOCK)
