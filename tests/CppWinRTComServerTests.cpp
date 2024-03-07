#include "pch.h"

#include "common.h"
#undef GetCurrentTime
#define WINRT_CUSTOM_MODULE_LOCK
#include <wil/cppwinrt_notifiable_module_lock.h>
#include <winrt/Windows.Foundation.h>
#include <wil/cppwinrt_register_com_server.h>
#include <wil/resource.h>
#include <string_view>
#include <thread>
#include <chrono>

using namespace std::string_view_literals;

wil::unique_event _comExit;

void notifier()
{
    _comExit.SetEvent();
}

struct MyServer : winrt::implements<MyServer, winrt::Windows::Foundation::IStringable>
{
    winrt::hstring ToString()
    {
        return L"MyServer from Server";
    }
};

struct BuggyServer : winrt::implements<BuggyServer, winrt::Windows::Foundation::IStringable>
{
    BuggyServer()
    {
        throw winrt::hresult_access_denied{};
    }
    winrt::hstring ToString()
    {
        return L"BuggyServer from Server";
    }
};

auto create_my_server_instance()
{
    return winrt::create_instance<winrt::Windows::Foundation::IStringable>(winrt::guid_of<MyServer>(), CLSCTX_LOCAL_SERVER);
}

TEST_CASE("CppWinRTComServerTests::DefaultNotifiableModuleLock", "[cppwinrt_com_server]")
{
    _comExit.create();

    wil::notifiable_module_lock::set_notifier(notifier);

    winrt::init_apartment();

    {
        auto server{winrt::make<MyServer>()};
        REQUIRE(winrt::get_module_lock() == 1);
    }

    _comExit.wait();

    REQUIRE(!winrt::get_module_lock());
}

TEST_CASE("CppWinRTComServerTests::RegisterComServer", "[cppwinrt_com_server]")
{
    winrt::init_apartment();

    {
        auto revoker = wil::register_com_server<MyServer>();
        auto instance = create_my_server_instance();
        REQUIRE(winrt::get_module_lock() == 1);
    }
    REQUIRE(!winrt::get_module_lock());
    try
    {
        auto instance = create_my_server_instance();
    }
    catch (winrt::hresult_error const& e)
    {
        REQUIRE(e.code() == REGDB_E_CLASSNOTREG);
    }
}

TEST_CASE("CppWinRTComServerTests::RegisterComServerThrowIsSafe", "[cppwinrt_com_server]")
{
    winrt::init_apartment();

    {
        auto revoker = wil::register_com_server<BuggyServer>();
        try
        {
            auto instance = winrt::create_instance<winrt::Windows::Foundation::IStringable>(winrt::guid_of<BuggyServer>(), CLSCTX_LOCAL_SERVER);
            REQUIRE(false);
        }
        catch (winrt::hresult_error const& e)
        {
            REQUIRE(e.code() == E_ACCESSDENIED);
        }
    }
}

winrt::Windows::Foundation::IAsyncAction create_instance_after_5_s()
{
    using namespace std::chrono_literals;
    co_await winrt::resume_after(5s);
    {
        try
        {
            auto instance = create_my_server_instance();
            // 2 because this coroutine also gets counted
            REQUIRE(winrt::get_module_lock() == 2);
        }
        catch (winrt::hresult_error const&)
        {
            REQUIRE(false);
        }
    }
}

TEST_CASE("CppWinRTComServerTests::NotifierAndRegistration", "[cppwinrt_com_server]")
{
    _comExit.create();

    wil::notifiable_module_lock::set_notifier(notifier);

    winrt::init_apartment();

    auto revoker = wil::register_com_server<MyServer>();

    create_instance_after_5_s();

    _comExit.wait();

    REQUIRE(!winrt::get_module_lock());
}
