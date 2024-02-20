#include "pch.h"

#include "common.h"
#undef GetCurrentTime
#if _MSVC_LANG >= 201703L
#include <wil/cppwinrt_notifiable_module_lock.h>
#include <winrt/Windows.Foundation.h>
#include <wil/cppwinrt_register_com_server.h>
#include <wil/resource.h>

wil::unique_event _comExit;

void notifier()
{
    _comExit.SetEvent();
}

struct MyServer : winrt::implements<MyServer, winrt::Windows::Foundation::IStringable>
{
};

TEST_CASE("CppWinRTComServerTests::DefaultNotifier", "[cppwinrt_com_server]")
{
    _comExit.create();

    wil::set_cppwinrt_module_notifier(notifier);

    winrt::init_apartment();

    auto revoker = wil::register_com_server<MyServer>();

    _comExit.wait();
}

#endif
