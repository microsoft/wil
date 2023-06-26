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

struct my_async_status : winrt::implements<my_async_status, winrt::Windows::Foundation::IAsyncInfo>
{
    wil::single_threaded_property<winrt::Windows::Foundation::AsyncStatus> Status{ winrt::Windows::Foundation::AsyncStatus::Started };
    wil::single_threaded_property<winrt::hresult> ErrorCode;
    wil::single_threaded_property<uint32_t> Id{ 16 };

    void Cancel() {};
    void Close() {};
};

TEST_CASE("CppWinRTAuthoringTests::Read", "[property]")
{
    int value = 42;
    wil::single_threaded_property<int> prop(value);
    REQUIRE(prop == value);
    REQUIRE(prop() == value);
    REQUIRE(prop == prop());
    REQUIRE(prop == prop);

    wil::single_threaded_property<int> prop2 = prop;
    REQUIRE(prop2 == value);
    REQUIRE(prop2() == value);
    REQUIRE(prop2 == prop());
    REQUIRE(prop2 == prop);

    wil::single_threaded_property<winrt::hstring> prop3;
    REQUIRE(prop3.empty());

    auto my_status = winrt::make<my_async_status>();
    REQUIRE(my_status.Status() == winrt::Windows::Foundation::AsyncStatus::Started);
    REQUIRE(my_status.ErrorCode() == S_OK);
    REQUIRE(my_status.Id() == 16);
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

    wil::single_threaded_rw_property<std::string> prop3("foo");
    REQUIRE(prop3 == "foo");
    REQUIRE(prop3() == "foo");
    REQUIRE(prop3.length() == 3);
    prop3 = "bar";
    REQUIRE(prop3 == "bar");
    auto& prop3alias = prop3("baz");
    REQUIRE(prop3 == "baz");
    prop3alias = "foo";
    REQUIRE(prop3 == "foo");
}

TEST_CASE("CppWinRTAuthoringTests::ReadWriteFromReadOnly", "[property]")
{
    int value = 42;
    wil::single_threaded_property<int> prop(value);
    REQUIRE(prop == value);
    REQUIRE(prop() == value);
    REQUIRE(prop == prop());
    REQUIRE(prop == prop);

    wil::single_threaded_rw_property<int> prop2(prop);
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

    wil::single_threaded_rw_property<int> prop3{ prop };
    REQUIRE(prop3 == value);
    REQUIRE(prop3() == value);
    REQUIRE(prop3 == prop());
    REQUIRE(prop3 == prop);

    wil::single_threaded_rw_property<int> prop4 = prop;
    REQUIRE(prop4 == value);
    REQUIRE(prop4() == value);
    REQUIRE(prop4 == prop());
    REQUIRE(prop4 == prop);
}

TEST_CASE("CppWinRTAuthoringTests::InStruct", "[property]")
{
    struct TestStruct
    {
        wil::single_threaded_property<int> Prop1{ 42 };
        wil::single_threaded_rw_property<int> Prop2{ 1 };
        wil::single_threaded_property<int> Prop3{ 44 };
        void foo()
        {
            Prop1 = -42;
        }
    };

    TestStruct test;
    test.Prop2 = 43;

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

#if defined(WINRT_Windows_UI_Xaml_Data_H)
#include <winrt/Windows.UI.Xaml.Hosting.h>

// This test cannot run in the same process as the malloc spies tests in wiTest.cpp
// MSFT_internal: https://task.ms/44191550
TEST_CASE("CppWinRTAuthoringTests::NotifyPropertyChanged", "[property][LocalOnly]")
{
#if defined(WIL_ENABLE_EXCEPTIONS)
    auto uninit = wil::RoInitialize_failfast(RO_INIT_SINGLETHREADED);
    // We need to initialize the XAML core in order to instantiate a PropertyChangedEventArgs.
    auto manager = winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
    {
        struct Test : winrt::implements<Test, winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged>, wil::notify_property_changed_base<Test>
        {
            wil::single_threaded_notifying_property<int> MyProperty;
            Test() : INIT_NOTIFYING_PROPERTY(MyProperty, 42) {}
        };
        auto test = winrt::make<Test>();
        auto testImpl = winrt::get_self<Test>(test);
        bool notified = false;
        auto token = test.PropertyChanged([&](winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs args)
            {
                REQUIRE(args.PropertyName() == L"MyProperty");
                REQUIRE(testImpl->MyProperty() == 43);
                notified = true;
            });

        testImpl->MyProperty(43);
        REQUIRE(notified);
        test.PropertyChanged(token);
        REQUIRE(testImpl->MyProperty.Name() == L"MyProperty");
    }
    {
        struct Test : winrt::implements<Test, winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged>, wil::notify_property_changed_base<Test>
        {
            WIL_NOTIFYING_PROPERTY(int, MyProperty, 42);
        };
        auto test = winrt::make<Test>();
        auto testImpl = winrt::get_self<Test>(test);
        bool notified = false;
        auto token = test.PropertyChanged([&](winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs args)
            {
                REQUIRE(args.PropertyName() == L"MyProperty");
                REQUIRE(testImpl->MyProperty() == 43);
                notified = true;
            });

        testImpl->MyProperty(43);
        REQUIRE(notified);
        test.PropertyChanged(token);
    }
    manager.Close();
#endif
}
#endif // msvc