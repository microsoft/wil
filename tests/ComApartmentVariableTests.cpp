#include <wil/com_apartment_variable.h>
#include <functional>

#include "common.h"

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

auto fn() { return 42; };
auto fn2() { return 43; };
auto fn3() { return 42; };

TEST_CASE("ComApartmentVariable::GetTests", "[com][get_for_current_com_apartment]")
{
    if (!wil::are_apartment_variables_supported()) return;

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
    if (!wil::are_apartment_variables_supported()) return;

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

    // variable not present, so this has no effect
    wil::reset_for_current_com_apartment(fn2, 42);
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

struct ApartmentVariableTester : public winrt::implements<ApartmentVariableTester, IUnknown>
{
    ApartmentVariableTester()
    {
        InstanceCount++;
    }
    ~ApartmentVariableTester()
    {
        InstanceCount--;
    }
    inline static int InstanceCount = 0;
};

auto create_apartment_tester()
{
    return winrt::make_self<ApartmentVariableTester>();
};

TEST_CASE("ComApartmentVariable::CheckInstanceLifetime", "[com][get_for_current_com_apartment]")
{
    if (!wil::are_apartment_variables_supported()) return;

    ApartmentVariableTester::InstanceCount = 0;
    {
        auto coUninit = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);

        REQUIRE(ApartmentVariableTester::InstanceCount == 0);
        auto var = wil::get_for_current_com_apartment(create_apartment_tester);
        REQUIRE(ApartmentVariableTester::InstanceCount == 1);
        auto var2 = wil::get_for_current_com_apartment(create_apartment_tester);
        REQUIRE(var.get() == var2.get());
        REQUIRE(ApartmentVariableTester::InstanceCount == 1);
    }
    REQUIRE(ApartmentVariableTester::InstanceCount == 0);

}

TEST_CASE("ComApartmentVariable::VerifyOnlyFunctionsArePassed", "[com][get_for_current_com_apartment]")
{
    if (!wil::are_apartment_variables_supported()) return;

    // Uncomment these lines to verify that they don't compile.
    auto lambda = []() { return 42; };
    // wil::get_for_current_com_apartment(lambda);

    std::function<bool()> function([]() { return 42; });
    // wil::get_for_current_com_apartment(function);
}

#endif