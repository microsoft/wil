#include "pch.h"

#include "common.h"
#undef GetCurrentTime
#define WINRT_CUSTOM_MODULE_LOCK
#include <wil/cppwinrt_notifiable_server_lock.h>
#include <winrt/Windows.Foundation.h>
#include <wil/resource.h>
#include <string_view>
#include <thread>
#include <chrono>

using namespace std::string_view_literals;

TEST_CASE("CppWinRTComServerTests::NotifiableServerLock", "[cppwinrt_com_server]")
{
    struct Test : winrt::implements<Test, IUnknown>
    {
    };

    bool released = false;

    wil::notifiable_server_lock::instance().set_notifier([&] {
        released = true;
    });
    auto resetOnExit = wil::scope_exit([] {
        wil::notifiable_server_lock::instance().set_notifier(nullptr);
    });

    winrt::init_apartment();

    {
        auto server{winrt::make<Test>()};
    }

    REQUIRE(released);
}
