#include <wil/com_apartment_variable.h>
#include <wil/com.h>
#include <functional>

#include "common.h"

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

auto fn() { return 42; };
auto fn2() { return 43; };
auto fn3() { return 42; };

TEST_CASE("ComApartmentVariable::GetTests", "[com][get_for_current_com_apartment]")
{
    if (!wil::are_apartment_variables_supported()) { INFO("skipping " __FUNCDNAME__); return; }

    {
        auto coUninit = wil::CoInitializeEx(COINIT_MULTITHREADED);
        auto v1 = wil::get_for_current_com_apartment(fn);
        REQUIRE(wil::apartment_variable_count() == 1);
        auto v2 = wil::get_for_current_com_apartment(fn);
        REQUIRE(wil::apartment_variable_count() == 1);
        REQUIRE(v1 == v2);
    }

    {
        auto coUninit = wil::CoInitializeEx(COINIT_MULTITHREADED);
        auto v1 = wil::get_for_current_com_apartment(fn);
        auto v2 = wil::get_for_current_com_apartment(fn3); // unique functions get their own variable
        REQUIRE(wil::apartment_variable_count() == 2);
        REQUIRE(v1 == v2);
        auto v3 = wil::get_for_current_com_apartment(fn2);
        REQUIRE(v1 != v3);
        REQUIRE(wil::apartment_variable_count() == 3);
    }

    {
        auto coUninit = wil::CoInitializeEx(COINIT_MULTITHREADED);
        auto v1 = wil::get_for_current_com_apartment(fn);
        REQUIRE(wil::apartment_count() == 1);
        REQUIRE(wil::apartment_variable_count() == 1);
        auto v2 = wil::get_for_current_com_apartment(fn2);
        REQUIRE(wil::apartment_count() == 1);
        REQUIRE(v1 == 42);
        REQUIRE(v2 == 43);
        REQUIRE(wil::apartment_variable_count() == 2);
    }

    REQUIRE(wil::apartment_count() == 0);

    {
        auto coUninit = wil::CoInitializeEx(COINIT_MULTITHREADED);
        wil::get_for_current_com_apartment(fn);
        REQUIRE(wil::apartment_count() == 1);
        REQUIRE(wil::apartment_variable_count() == 1);

        std::thread([]()
        {
            auto coUninit = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
            wil::get_for_current_com_apartment(fn);
            REQUIRE(wil::apartment_count() == 2);
            REQUIRE(wil::apartment_variable_count() == 1);
        }).join();
        REQUIRE(wil::apartment_count() == 1);

        REQUIRE(wil::apartment_variable_count() == 1);
    }

    REQUIRE(wil::apartment_count() == 0);
}

TEST_CASE("ComApartmentVariable::ResetTests", "[com][reset_for_current_com_apartment]")
{
    if (!wil::are_apartment_variables_supported()) { INFO("skipping " __FUNCDNAME__); return; }

    auto coUninit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    auto v = wil::get_for_current_com_apartment(fn);
    REQUIRE(wil::apartment_variable_count() == 1);

    wil::reset_for_current_com_apartment(fn, 43);
    REQUIRE(wil::apartment_variable_count() == 1);
    v = wil::get_for_current_com_apartment(fn);
    REQUIRE(v == 43);

    wil::reset_for_current_com_apartment(fn);
    wil::reset_for_current_com_apartment(fn);
    REQUIRE(wil::apartment_variable_count() == 0);

    // No variable so nop
    wil::reset_for_current_com_apartment(fn2);

    // variable not present, will result in a fail fast
    // wil::reset_for_current_com_apartment(fn2, 42);
    v = wil::get_for_current_com_apartment(fn2);
    REQUIRE(v == 43);

    REQUIRE(wil::apartment_variable_count() == 1);

    wil::reset_for_current_com_apartment(fn2);
    REQUIRE(wil::apartment_variable_count() == 0);

    v = wil::get_for_current_com_apartment(fn2);
    wil::reset_for_current_com_apartment(fn2, 44);
    REQUIRE(wil::apartment_variable_count() == 1);

    v = wil::get_for_current_com_apartment(fn2);
    REQUIRE(wil::apartment_variable_count() == 1);
    REQUIRE(v == 44);
}

struct mock_platform
{
    static unsigned long long GetApartmentId()
    {
        APTTYPE aptType;
        APTTYPEQUALIFIER aptQualifer;
        FAIL_FAST_IF_FAILED(CoGetApartmentType(&aptType, &aptQualifer)); // ensure COM is inited

        // incorrect, but close enough for testing
        return GetCurrentThreadId();
    }

    inline static std::unordered_map<unsigned long long, std::vector<wil::com_ptr<IApartmentShutdown>>> m_observers;

    static void RegisterForApartmentShutdown(IApartmentShutdown* observer)
    {
        const auto id = GetApartmentId();
        auto apt_observers = m_observers.find(id);
        if (apt_observers == m_observers.end())
        {
            m_observers.insert({ id, { observer} });
        }
        else
        {
            apt_observers->second.push_back(observer);
        }
    }

    // This is needed to simulate the platform for unit testing.
    static auto CoInitializeExForTesting(DWORD coinitFlags = 0 /*COINIT_MULTITHREADED*/)
    {
        return wil::scope_exit([aptId = GetCurrentThreadId(), init = wil::CoInitializeEx(coinitFlags)]()
        {
            const auto id = GetApartmentId();
            auto apt_observers = m_observers.find(id);
            if (apt_observers != m_observers.end())
            {
                const auto& observers = apt_observers->second;
                for (auto& comptr : observers)
                {
                    comptr->OnUninitialize(id);
                }
            }
        });
    }
};


wil::apartment_variable<int, mock_platform> g_v1;

TEST_CASE("ComApartmentVariable::VerifyApartmentVariable", "[com][apartment_variable]")
{
    auto coUninit = mock_platform::CoInitializeExForTesting(COINIT_MULTITHREADED);

    std::ignore = g_v1.get_or_create(fn);

    wil::apartment_variable<int, mock_platform> v1;

    REQUIRE(v1.get_if() == nullptr);
    REQUIRE(v1.get_or_create(fn) == 42);
    v1.set(43);
    REQUIRE(v1.get_or_create(fn) == 43);
    REQUIRE(v1.get_existing() == 43);
    v1.clear();
    REQUIRE(v1.get_if() == nullptr);
}

TEST_CASE("ComApartmentVariable::VerifyApartmentVariableLifetimes", "[com][apartment_variable]")
{
    auto coUninit = mock_platform::CoInitializeExForTesting(COINIT_MULTITHREADED);

    wil::apartment_variable<int, mock_platform> av1, av2;

    // auto fn() { return 42; };
    // auto fn2() { return 43; };

    {
        auto coUninitInner = mock_platform::CoInitializeExForTesting(COINIT_MULTITHREADED);

        auto v1 = av1.get_or_create(fn);
        REQUIRE(av1.apartment_count() == 1);
        auto v2 = av1.get_existing();
        REQUIRE((av1.current_apartment_variable_count() == 1));
        REQUIRE(v1 == v2);
    }

    {
        auto coUninitInner = mock_platform::CoInitializeExForTesting(COINIT_MULTITHREADED);
        auto v1 = av1.get_or_create(fn);
        auto v2 = av2.get_or_create(fn2);
        REQUIRE((av1.current_apartment_variable_count() == 2));
        REQUIRE(v1 != v2);
        REQUIRE(av1.apartment_count() == 1);
    }

    REQUIRE((wil::apartment_variable<int, mock_platform>::apartment_count() == 0));

    {
        auto coUninitInner = mock_platform::CoInitializeExForTesting(COINIT_MULTITHREADED);

        auto v = av1.get_or_create(fn);
        REQUIRE(av1.current_apartment_variable_count() == 1);

        std::thread([&]() // join below makes this ok
        {
            auto coUninit = mock_platform::CoInitializeExForTesting(COINIT_APARTMENTTHREADED);
            std::ignore = av1.get_or_create(fn);
            REQUIRE(av1.apartment_count() == 2);
            REQUIRE(av1.current_apartment_variable_count() == 1);
        }).join();
        REQUIRE(av1.apartment_count() == 1);

        av1.get_or_create(fn)++;
        v = av1.get_existing();
        REQUIRE(v == 43);
    }

    {
        auto coUninitInner = mock_platform::CoInitializeExForTesting(COINIT_MULTITHREADED);

        std::ignore = av1.get_or_create(fn);
        REQUIRE(av1.current_apartment_variable_count() == 1);
        av1.set(1);
        av1.clear();
        REQUIRE(av1.current_apartment_variable_count() == 0);

        // will fail fast since clear() was called.
        // av1.set(1);
    }

    REQUIRE((wil::apartment_variable<int, mock_platform>::apartment_count() == 0));
}

#endif