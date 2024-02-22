#include "pch.h"

#include "common.h"
#undef GetCurrentTime
#define WIL_CPPWINRT_COM_SERVER_CUSTOM_MODULE_LOCK
#include <wil/cppwinrt_notifiable_module_lock.h>
namespace winrt
{
inline auto& get_module_lock()
{
    // Use a different signature to test that it isn't the default one
    static wil::notifiable_module_lock<int (*)()> lock;
    return lock;
}
} // namespace winrt
#include <winrt/Windows.Foundation.h>
#include <wil/cppwinrt_register_com_server.h>
#include <wil/resource.h>
#include <string_view>
#include <thread>
#include <chrono>

using namespace std::string_view_literals;

wil::unique_event _comExit;

int notifier()
{
    _comExit.SetEvent();
    return 0;
}

struct MyServer : winrt::implements<MyServer, winrt::Windows::Foundation::IStringable>
{
    winrt::hstring ToString()
    {
        return L"MyServer from Server";
    }
};

auto create_my_server_instance()
{
    return winrt::create_instance<winrt::Windows::Foundation::IStringable>(winrt::guid_of<MyServer>(), CLSCTX_LOCAL_SERVER);
}

TEST_CASE("CppWinRTComServerTests::DefaultNotifiableModuleLock", "[cppwinrt_com_server]")
{
    _comExit.create();

    wil::notifiable_module_lock<int (*)()>::set_notifier(notifier);

    winrt::init_apartment();

    {
        auto server{winrt::make<MyServer>()};
        REQUIRE(winrt::get_module_lock() == 1);
    }

    _comExit.wait();

    REQUIRE(!winrt::get_module_lock());
}
