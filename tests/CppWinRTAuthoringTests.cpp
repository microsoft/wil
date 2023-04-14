#define WINAPI_PARTITION_DESKTOP 1 // for RO_INIT_SINGLETHREADED
#include "common.h"
#undef GetCurrentTime
// check if at least C++17
#if _MSVC_LANG >= 201703L
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#endif

#include <wil/cppwinrt_authoring.h>
#include <wil/winrt.h>
#include <wil/resource.h>

TEST_CASE("CppWinRTAuthoringTests::ReadOnly", "[property]")
{
    int value = 42;
    wil::single_threaded_ro_property<int> prop(value);
    REQUIRE(prop == value);
    REQUIRE(prop() == value);
    REQUIRE(prop == prop());
    REQUIRE(prop == prop);

    wil::single_threaded_ro_property<int> prop2 = prop;
    REQUIRE(prop2 == value);
    REQUIRE(prop2() == value);
    REQUIRE(prop2 == prop());
    REQUIRE(prop2 == prop);
}

TEST_CASE("CppWinRTAuthoringTests::ReadWrite", "[property]")
{
    int value = 42;
    wil::single_threaded_rw_property<int> prop(value);
    REQUIRE(prop == value);
    REQUIRE(prop() == value);
    REQUIRE(prop == prop());
    REQUIRE(prop == prop);

    wil::single_threaded_rw_property<int> prop2 = prop;
    REQUIRE(prop2 == value);
    REQUIRE(prop2() == value);
    REQUIRE(prop2 == prop());
    REQUIRE(prop2 == prop);

    int value2 = 43;
    prop2 = value2;
    REQUIRE(prop2 == value2);
    REQUIRE(prop2() == value2);
    REQUIRE(prop2 == prop2());
    REQUIRE(prop2 == prop2);
}

TEST_CASE("CppWinRTAuthoringTests::ReadWriteFromReadOnly", "[property]")
{
    int value = 42;
    wil::single_threaded_ro_property<int> prop(value);
    REQUIRE(prop == value);
    REQUIRE(prop() == value);
    REQUIRE(prop == prop());
    REQUIRE(prop == prop);

    wil::single_threaded_rw_property<int> prop2 = prop;
    REQUIRE(prop2 == value);
    REQUIRE(prop2() == value);
    REQUIRE(prop2 == prop());
    REQUIRE(prop2 == prop);

    int value2 = 43;
    prop2 = value2;
    REQUIRE(prop2 == value2);
    REQUIRE(prop2() == value2);
    REQUIRE(prop2 == prop2());
    REQUIRE(prop2 == prop2);
}

TEST_CASE("CppWinRTAuthoringTests::InStruct", "[property]")
{
    struct TestStruct
    {
        wil::single_threaded_ro_property<int> Prop1{ 42 };
        wil::single_threaded_rw_property<int> Prop2{ 1 };
        wil::single_threaded_ro_property<int> Prop3{ 44 };
    };

    TestStruct test;
    static_assert(!std::is_assignable_v<wil::single_threaded_ro_property<int>, int>, "cannot assign to a readonly property");

    test.Prop2 = 43;
    static_assert(!std::is_assignable_v<decltype(test.Prop3), decltype(44)>, "cannot assign to a readonly property");

    REQUIRE(test.Prop1 == 42);
    REQUIRE(test.Prop2 == 43);
    REQUIRE(test.Prop3 == 44);

    test.Prop2 = 45;
    REQUIRE(test.Prop2 == 45);

    REQUIRE(test.Prop1() == 42);
    test.Prop2(99);
    REQUIRE(test.Prop2() == 99);
    test.Prop2(22)(33);
    REQUIRE(test.Prop2() == 33);
}

#ifdef WINRT_Windows_Foundation_H
TEST_CASE("CppWinRTAuthoringTests::Events", "[property]")
{
    struct Test
    {
        wil::simple_event<int> MyEvent;

        wil::typed_event<winrt::Windows::Foundation::IInspectable, int> MyTypedEvent;
    } test;

    auto token = test.MyEvent([](winrt::Windows::Foundation::IInspectable, int args) { REQUIRE(args == 42); });
    test.MyEvent.invoke(nullptr, 42);
    test.MyEvent(token);

    auto token2 = test.MyTypedEvent([](winrt::Windows::Foundation::IInspectable, int args) { REQUIRE(args == 42); });
    test.MyTypedEvent.invoke(nullptr, 42);
    test.MyTypedEvent(token2);
}
#endif // WINRT_Windows_Foundation_H

// if using MSVC since we need the embedded manifest bits to use XAML islands
#if _MSVC_LANG >= 201703L && defined(WINRT_Windows_UI_Xaml_Data_H)

#include <winrt/Windows.UI.Xaml.Hosting.h>

TEST_CASE("CppWinRTAuthoringTests::NotifyPropertyChanged", "[property]")
{
#if defined(WIL_ENABLE_EXCEPTIONS)
    auto uninit = wil::RoInitialize_failfast(RO_INIT_SINGLETHREADED);
    // We need to initialize the XAML core in order to instantiate a PropertyChangedEventArgs.
    // XAML needs to be unloaded before COM rundown.
    auto xaml = wil::unique_hmodule(::LoadLibraryExW(L"Windows.UI.Xaml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    auto manager = winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();

    struct Test : winrt::implements<Test, winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged>, wil::notify_property_changed_base<Test>
    {
      using wil::notify_property_changed_base<Test>::PropertyChanged;
      wil::single_threaded_notifying_property<int> MyProperty;
      Test() : INIT_NOTIFY_PROPERTY(MyProperty, 42) {}
    };
    auto test = winrt::make<Test>();
    auto testImpl = winrt::get_self<Test>(test);

    auto token = test.PropertyChanged([&](winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs args)
    {
        REQUIRE(args.PropertyName() == L"MyProperty");
        REQUIRE(testImpl->MyProperty() == 43);
    });

    testImpl->MyProperty(43);
    test.PropertyChanged(token);

    manager.Close();
#endif
}
#endif // msvc