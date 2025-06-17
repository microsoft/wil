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

    wil::notifiable_module_lock::instance().set_notifier(notifier);
    auto resetOnExit = wil::scope_exit([&] {
        wil::notifiable_module_lock::instance().set_notifier(nullptr);
    });

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
            auto instance =
                winrt::create_instance<winrt::Windows::Foundation::IStringable>(winrt::guid_of<BuggyServer>(), CLSCTX_LOCAL_SERVER);
            REQUIRE(false);
        }
        catch (winrt::hresult_error const& e)
        {
            REQUIRE(e.code() == E_ACCESSDENIED);
        }
    }
}

TEST_CASE("CppWinRTComServerTests::AnyRegisterFailureClearAllRegistrations", "[cppwinrt_com_server]")
{
    winrt::init_apartment();

    witest::detoured_thread_function<&::CoRegisterClassObject> detour(
        [](REFCLSID clsid, LPUNKNOWN obj, DWORD ctxt, DWORD flags, LPDWORD cookie) mutable {
            if (winrt::guid{clsid} == winrt::guid_of<BuggyServer>())
            {
                *cookie = 0;
                return E_UNEXPECTED;
            }
            return ::CoRegisterClassObject(clsid, obj, ctxt, flags, cookie);
        });
    try
    {
        auto revoker = wil::register_com_server<MyServer, BuggyServer>();
        REQUIRE(false);
    }
    catch (winrt::hresult_error const& e)
    {
        REQUIRE(e.code() == E_UNEXPECTED);
    }
    REQUIRE(!winrt::get_module_lock());
}

TEST_CASE("CppWinRTComServerTests::NotifierAndRegistration", "[cppwinrt_com_server]")
{
    wil::unique_event moduleEvent(wil::EventOptions::ManualReset);
    wil::unique_event coroutineRunning(wil::EventOptions::ManualReset);
    wil::unique_event coroutineContinue(wil::EventOptions::ManualReset);

    wil::notifiable_module_lock::instance().set_notifier([&]() {
        moduleEvent.SetEvent();
    });
    auto resetOnExit = wil::scope_exit([&] {
        wil::notifiable_module_lock::instance().set_notifier(nullptr);
    });

    winrt::init_apartment();

    auto revoker = wil::register_com_server<MyServer>();

    std::exception_ptr coroutineException;
    auto asyncLambda = [&]() -> winrt::Windows::Foundation::IAsyncAction {
        try
        {
            co_await winrt::resume_background();
            coroutineRunning.SetEvent();

            coroutineContinue.wait();
            auto instance = create_my_server_instance();
            REQUIRE(winrt::get_module_lock() == 2);
        }
        catch (...)
        {
            coroutineException = std::current_exception();
        }
    };
    asyncLambda();

    coroutineRunning.wait();
    REQUIRE(winrt::get_module_lock() == 1); // Coroutine bumped count

    coroutineContinue.SetEvent();
    moduleEvent.wait();

    if (coroutineException)
    {
        std::rethrow_exception(coroutineException);
    }

    REQUIRE(!winrt::get_module_lock());
}
