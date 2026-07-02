#include "pch.h"

// Just verify that Tracelogging.h compiles.
#define PROVIDER_CLASS_NAME TestProvider
#include "TraceLoggingTests.h"

#include "catch.hpp"
#include "common.h"

TEST_CASE("TraceLoggingTests::ActivitySuspendResume", "[tracelogging]")
{
    // Test that Activity classes implement the suspend/resume interface for coroutine watchers.
    // This interface is used by wil::with_watcher() to pause error watching during co_await.
    auto activity = TestProvider::TraceloggingActivity::Start();

    // Initially watching after Start()
    REQUIRE(activity.IsRunning());

    // suspend() should return true (was watching) and stop watching
    bool wasWatching = activity.suspend();
    REQUIRE(wasWatching);
    REQUIRE_FALSE(activity.IsWatching());

    // Calling suspend() again should return false (wasn't watching)
    wasWatching = activity.suspend();
    REQUIRE_FALSE(wasWatching);
    REQUIRE_FALSE(activity.IsWatching());

    // resume() should restart watching
    activity.resume();
    REQUIRE(activity.IsWatching());

    // Calling resume() when already watching is safe
    activity.resume();
    REQUIRE(activity.IsWatching());

    activity.Stop();
}
