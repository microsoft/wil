
#include "catch.hpp"

#include <wil/cppwinrt.h>

TEST_CASE("CppWinRTTests::WilToCppWinRTExceptionTranslationTest", "[cppwinrt]")
{
    auto test = [](HRESULT hr)
    {
        try
        {
            THROW_HR(hr);
        }
        catch (...)
        {
            REQUIRE(hr == winrt::to_hresult());
        }
    };

    test(E_UNEXPECTED);
    test(E_ACCESSDENIED);
    test(E_INVALIDARG);
    test(E_HANDLE);
    test(E_OUTOFMEMORY);
}

TEST_CASE("CppWinRTTests::CppWinRTToWilExceptionTranslationTest", "[cppwinrt]")
{
    auto test = [](HRESULT hr)
    {
        try
        {
            winrt::check_hresult(hr);
        }
        catch (...)
        {
            REQUIRE(hr == wil::ResultFromCaughtException());
        }
    };

    test(E_UNEXPECTED);
    test(E_ACCESSDENIED);
    test(E_INVALIDARG);
    test(E_HANDLE);
    test(E_OUTOFMEMORY);
}

TEST_CASE("CppWinRTTests::ResultFromExceptionDebugTest", "[cppwinrt]")
{
    auto test = [](HRESULT hr, wil::SupportedExceptions supportedExceptions)
    {
        auto result = wil::ResultFromExceptionDebug(WI_DIAGNOSTICS_INFO, supportedExceptions, [&]()
        {
            winrt::check_hresult(hr);
        });
        REQUIRE(hr == result);
    };

    // Anything from SupportedExceptions::Known or SupportedExceptions::All should give back the same HRESULT
    test(E_UNEXPECTED, wil::SupportedExceptions::Known);
    test(E_ACCESSDENIED, wil::SupportedExceptions::Known);
    test(E_INVALIDARG, wil::SupportedExceptions::All);
    test(E_HANDLE, wil::SupportedExceptions::All);

    // OOM gets translated to bad_alloc, which should always give back E_OUTOFMEMORY
    test(E_OUTOFMEMORY, wil::SupportedExceptions::All);
    test(E_OUTOFMEMORY, wil::SupportedExceptions::Known);
    test(E_OUTOFMEMORY, wil::SupportedExceptions::ThrownOrAlloc);

    // Uncomment any of the following to validate SEH failfast
    //test(E_UNEXPECTED, wil::SupportedExceptions::None);
    //test(E_ACCESSDENIED, wil::SupportedExceptions::Thrown);
    //test(E_INVALIDARG, wil::SupportedExceptions::ThrownOrAlloc);
}
