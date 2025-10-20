#include "pch.h"

#include <wil/com_apartment_variable.h>
#include <wil/com.h>
#include <functional>

#include "common.h"
#include "cppwinrt_threadpool_guard.h"

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

template <typename... args_t>
static inline void LogOutput(_Printf_format_string_ PCWSTR format, args_t&&... args)
{
    OutputDebugStringW(wil::str_printf_failfast<wil::unique_cotaskmem_string>(format, wistd::forward<args_t>(args)...).get());
}

static inline bool IsComInitialized()
{
    APTTYPE type{};
    APTTYPEQUALIFIER qualifier{};
    return CoGetApartmentType(&type, &qualifier) == S_OK;
}

static inline void WaitForAllComApartmentsToRundown()
{
    while (IsComInitialized())
    {
        Sleep(0);
    }
}

static void co_wait(const wil::unique_event& evt)
{
    HANDLE raw[] = {evt.get()};
    ULONG index{};
    REQUIRE_SUCCEEDED(CoWaitForMultipleHandles(COWAIT_DISPATCH_CALLS, INFINITE, static_cast<ULONG>(std::size(raw)), raw, &index));
}

static void RunApartmentVariableTest(void (*test)())
{
    {
        cppwinrt_threadpool_guard guard;
        test();
    }

    // Apartment variable rundown is async, wait for the last COM apartment
    // to rundown before proceeding to the next test.
    WaitForAllComApartmentsToRundown();
}

struct mock_platform
{
    static unsigned long long GetApartmentId()
    {
        APTTYPE type;
        APTTYPEQUALIFIER qualifier;
        REQUIRE_SUCCEEDED(CoGetApartmentType(&type, &qualifier)); // ensure COM is inited

        // Approximate apartment Id
        if (type == APTTYPE_STA)
        {
            REQUIRE_FALSE(GetCurrentThreadId() < APTTYPE_MAINSTA);
            return GetCurrentThreadId();
        }

        // APTTYPE_MTA (1), APTTYPE_NA (2), APTTYPE_MAINSTA (3)
        return type;
    }

    static auto RegisterForApartmentShutdown(IApartmentShutdown* observer)
    {
        const auto id = GetApartmentId();
        auto apt_observers = m_observers.find(id);
        if (apt_observers == m_observers.end())
        {
            m_observers.insert({id, {observer}});
        }
        else
        {
            apt_observers->second.emplace_back(observer);
        }
        // NOLINTNEXTLINE(performance-no-int-to-ptr): Cookie masquerading as a pointer
        return shutdown_type{reinterpret_cast<APARTMENT_SHUTDOWN_REGISTRATION_COOKIE>(id)};
    }

    static void UnRegisterForApartmentShutdown(APARTMENT_SHUTDOWN_REGISTRATION_COOKIE cookie)
    {
        auto id = reinterpret_cast<unsigned long long>(cookie);
        m_observers.erase(id);
    }

    using shutdown_type =
        wil::unique_any<APARTMENT_SHUTDOWN_REGISTRATION_COOKIE, decltype(&UnRegisterForApartmentShutdown), UnRegisterForApartmentShutdown>;

    // This is needed to simulate the platform for unit testing.
    static auto CoInitializeEx(DWORD coinitFlags = 0 /*COINIT_MULTITHREADED*/)
    {
        return wil::scope_exit([aptId = GetCurrentThreadId(), init = wil::CoInitializeEx(coinitFlags)]() {
            const auto id = GetApartmentId();
            auto apt_observers = m_observers.find(id);
            if (apt_observers != m_observers.end())
            {
                const auto& observers = apt_observers->second;
                for (auto& observer : observers)
                {
                    observer->OnUninitialize(id);
                }
                m_observers.erase(apt_observers);
            }
        });
    }

    // Enable the test hook to force losing the race
    static constexpr unsigned long AsyncRundownDelayForTestingRaces = 1; // enable test hook
    inline static std::unordered_map<unsigned long long, std::vector<wil::com_ptr<IApartmentShutdown>>> m_observers;
};

static auto fn()
{
    return 42;
};
static auto fn2()
{
    return 43;
};

static wil::apartment_variable<int, wil::apartment_variable_leak_action::ignore, mock_platform> g_v1;
static wil::apartment_variable<int, wil::apartment_variable_leak_action::ignore> g_v2;

template <typename platform = wil::apartment_variable_platform>
static void TestApartmentVariableAllMethods()
{
    auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

    std::ignore = g_v1.get_or_create(fn);
    auto clearOnExit = wil::scope_exit([&] {
        // Other tests rely on the C++/WinRT module lock count being zero at the start. Ensure we clean up after ourselves
        g_v1.clear();
    });

    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> var;

    REQUIRE(var.get_if() == nullptr);
    REQUIRE(var.get_or_create(fn) == 42);
    int varue = 43;
    var.set(varue);
    REQUIRE(var.get_or_create(fn) == 43);
    REQUIRE(var.get_existing() == 43);
    var.clear();
    REQUIRE(var.get_if() == nullptr);
}

template <typename platform = wil::apartment_variable_platform>
static void TestApartmentVariableGetOrCreateForms()
{
    auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> var;
    REQUIRE(var.get_or_create(fn) == 42);
    var.clear();
    REQUIRE(var.get_or_create([&] {
        return 1;
    }) == 1);
    var.clear();
    REQUIRE(var.get_or_create() == 0);
}

template <typename platform = wil::apartment_variable_platform>
static void TestApartmentVariableLifetimes()
{
    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> av1;
    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> av2;

    {
        auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

        auto val1 = av1.get_or_create(fn);
        REQUIRE(av1.storage().size() == 1);
        auto val2 = av1.get_existing();
        REQUIRE(av1.current_apartment_variable_count() == 1);
        REQUIRE(val1 == val2);
    }

    {
        auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);
        auto val1 = av1.get_or_create(fn);
        auto val2 = av2.get_or_create(fn2);
        REQUIRE((av1.current_apartment_variable_count() == 2));
        REQUIRE(val1 != val2);
        REQUIRE(av1.storage().size() == 1);
    }

    REQUIRE(av1.storage().size() == 0);

    {
        auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

        auto val = av1.get_or_create(fn);
        REQUIRE(av1.current_apartment_variable_count() == 1);

        std::thread([&]() // join below makes this ok
                    {
                        SetThreadDescription(GetCurrentThread(), L"STA");
                        auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
                        std::ignore = av1.get_or_create(fn);
                        REQUIRE(av1.storage().size() == 2);
                        REQUIRE(av1.current_apartment_variable_count() == 1);
                    })
            .join();
        REQUIRE(av1.storage().size() == 1);

        av1.get_or_create(fn)++;
        val = av1.get_existing();
        REQUIRE(val == 43);
    }

    {
        auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

        std::ignore = av1.get_or_create(fn);
        REQUIRE(av1.current_apartment_variable_count() == 1);
        int newVal = 1;
        av1.set(newVal);
        av1.clear();
        REQUIRE(av1.current_apartment_variable_count() == 0);

        // will fail fast since clear() was called.
        // av1.set(1);
        av1.clear_all_apartments_async().get();
    }

    REQUIRE(av1.storage().size() == 0);
}

template <typename platform = wil::apartment_variable_platform>
static void TestMultipleApartments()
{
    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> av1;
    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> av2;

    wil::unique_event t1Created{wil::EventOptions::None};
    wil::unique_event t2Created{wil::EventOptions::None};
    wil::unique_event t1Shutdown{wil::EventOptions::None};
    wil::unique_event t2Shutdown{wil::EventOptions::None};

    auto apt1_thread = std::thread([&]() // join below makes this ok
                                   {
                                       SetThreadDescription(GetCurrentThread(), L"STA 1");
                                       auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
                                       std::ignore = av1.get_or_create(fn);
                                       std::ignore = av2.get_or_create(fn);
                                       t1Created.SetEvent();
                                       co_wait(t1Shutdown);
                                   });

    auto apt2_thread = std::thread([&]() // join below makes this ok
                                   {
                                       SetThreadDescription(GetCurrentThread(), L"STA 2");
                                       auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
                                       std::ignore = av1.get_or_create(fn);
                                       std::ignore = av2.get_or_create(fn);
                                       t2Created.SetEvent();
                                       co_wait(t2Shutdown);
                                   });

    t1Created.wait();
    t2Created.wait();
    av1.clear_all_apartments_async().get();
    av2.clear_all_apartments_async().get();

    t1Shutdown.SetEvent();
    t2Shutdown.SetEvent();

    apt1_thread.join();
    apt2_thread.join();

    REQUIRE((wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform>::storage().size() == 0));
}

template <typename platform = wil::apartment_variable_platform>
static void TestWinningApartmentAlreadyRundownRace()
{
    auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> aptVar;

    std::ignore = aptVar.get_or_create(fn);
    const auto& storage = aptVar.storage(); // for viewing the storage in the debugger

    wil::unique_event otherAptVarCreated{wil::EventOptions::None};
    wil::unique_event startApartmentRundown{wil::EventOptions::None};
    wil::unique_event comRundownComplete{wil::EventOptions::None};

    auto apt_thread = std::thread([&]() // join below makes this ok
                                  {
                                      SetThreadDescription(GetCurrentThread(), L"STA");
                                      auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
                                      std::ignore = aptVar.get_or_create(fn);
                                      otherAptVarCreated.SetEvent();
                                      co_wait(startApartmentRundown);
                                  });

    otherAptVarCreated.wait();
    // we now have aptVar in this apartment and in the STA
    REQUIRE(storage.size() == 2);
    // wait for async clean to complete
    aptVar.clear_all_apartments_async().get();
    startApartmentRundown.SetEvent();

    REQUIRE(aptVar.storage().size() == 0);
    apt_thread.join();
}

template <typename platform = wil::apartment_variable_platform>
static void TestLosingApartmentAlreadyRundownRace()
{
    auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> aptVar;

    std::ignore = aptVar.get_or_create(fn);
    const auto& storage = aptVar.storage(); // for viewing the storage in the debugger

    wil::unique_event otherAptVarCreated{wil::EventOptions::None};
    wil::unique_event startApartmentRundown{wil::EventOptions::None};
    wil::unique_event comRundownComplete{wil::EventOptions::None};

    auto apt_thread = std::thread([&]() // join below makes this ok
                                  {
                                      SetThreadDescription(GetCurrentThread(), L"STA");
                                      auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
                                      std::ignore = aptVar.get_or_create(fn);
                                      otherAptVarCreated.SetEvent();
                                      co_wait(startApartmentRundown);
                                      coUninit.reset();
                                      comRundownComplete.SetEvent();
                                  });

    otherAptVarCreated.wait();
    // we now have aptVar in this apartment and in the STA
    REQUIRE(storage.size() == 2);
    auto clearAllOperation = aptVar.clear_all_apartments_async();
    startApartmentRundown.SetEvent();
    comRundownComplete.wait();
    clearAllOperation.get(); // wait for the async rundowns to complete

    REQUIRE(aptVar.storage().size() == 0);
    apt_thread.join();
}

TEST_CASE("ComApartmentVariable::ShutdownRegistration", "[LocalOnly][com][unique_apartment_shutdown_registration]")
{
    {
        wil::unique_apartment_shutdown_registration apt_shutdown_registration;
    }

    {
        auto coUninit = wil::CoInitializeEx(COINIT_MULTITHREADED);

        struct ApartmentObserver : public winrt::implements<ApartmentObserver, IApartmentShutdown>
        {
            void STDMETHODCALLTYPE OnUninitialize(unsigned long long apartmentId) noexcept override
            {
                LogOutput(L"OnUninitialize %ull\n", apartmentId);
            }
        };

        wil::unique_apartment_shutdown_registration apt_shutdown_registration;
        unsigned long long id{};
        REQUIRE_SUCCEEDED(::RoRegisterForApartmentShutdown(winrt::make<ApartmentObserver>().get(), &id, apt_shutdown_registration.put()));
        LogOutput(L"RoRegisterForApartmentShutdown %p\r\n", apt_shutdown_registration.get());
        // don't unregister and let the pending COM apartment rundown invoke the callback.
        apt_shutdown_registration.release();
    }
}

TEST_CASE("ComApartmentVariable::CallAllMethods", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestApartmentVariableAllMethods<mock_platform>);
}

TEST_CASE("ComApartmentVariable::GetOrCreateForms", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestApartmentVariableGetOrCreateForms<mock_platform>);
}

TEST_CASE("ComApartmentVariable::VariableLifetimes", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestApartmentVariableLifetimes<mock_platform>);
}

TEST_CASE("ComApartmentVariable::WinningApartmentAlreadyRundownRace", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestWinningApartmentAlreadyRundownRace<mock_platform>);
}

TEST_CASE("ComApartmentVariable::LosingApartmentAlreadyRundownRace", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestLosingApartmentAlreadyRundownRace<mock_platform>);
}

TEST_CASE("ComApartmentVariable::MultipleApartments", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestMultipleApartments<mock_platform>);
}

TEST_CASE("ComApartmentVariable::UseRealPlatformRunAllTests", "[com][apartment_variable]")
{
    if (!wil::are_apartment_variables_supported())
    {
        return;
    }

    RunApartmentVariableTest(TestApartmentVariableAllMethods);
    RunApartmentVariableTest(TestApartmentVariableGetOrCreateForms);
    RunApartmentVariableTest(TestApartmentVariableLifetimes);
    RunApartmentVariableTest(TestWinningApartmentAlreadyRundownRace);
    RunApartmentVariableTest(TestLosingApartmentAlreadyRundownRace);
    RunApartmentVariableTest(TestMultipleApartments);
}

#endif
