#include "pch.h"

#include <wil/com.h>
#include <wil/result.h>

#if (NTDDI_VERSION >= NTDDI_WIN8)
#include <wil/result_originate.h>
#include <wil/result_macros.h>
#endif

#include <roerrorapi.h>

#include "common.h"

static volatile long objectCount = 0;
struct SharedObject
{
    SharedObject()
    {
        ::InterlockedIncrement(&objectCount);
    }

    ~SharedObject()
    {
        ::InterlockedDecrement(&objectCount);
    }

    void ProcessShutdown()
    {
    }

    int value{};
};

TEST_CASE("ResultTests::SemaphoreValue", "[result]")
{
    auto TestValue = [&](auto start, auto end) {
        wil::details_abi::SemaphoreValue semaphore;
        for (auto index = start; index <= end; index++)
        {
            semaphore.Destroy();
            REQUIRE(SUCCEEDED(semaphore.CreateFromValue(L"test", index)));

            auto num1 = index;
            auto num2 = index;
            REQUIRE(SUCCEEDED(semaphore.TryGetValue(L"test", &num1)));
            REQUIRE(SUCCEEDED(semaphore.TryGetValue(L"test", &num2)));
            REQUIRE(num1 == index);
            REQUIRE(num2 == index);
        }
    };

    // Test 32-bit values (edge cases)
    TestValue(0u, 10u);
    TestValue(250u, 260u);
    TestValue(0x7FFFFFF0u, 0x7FFFFFFFu);

    // Test 64-bit values (edge cases)
    TestValue(0ull, 10ull);
    TestValue(250ull, 260ull);
    TestValue(0x000000007FFFFFF0ull, 0x000000008000000Full);
    TestValue(0x00000000FFFFFFF0ull, 0x000000010000000Full);
    TestValue(0x00000000FFFFFFF0ull, 0x000000010000000Full);
    TestValue(0x3FFFFFFFFFFFFFF0ull, 0x3FFFFFFFFFFFFFFFull);

    // Test pointer values
    wil::details_abi::SemaphoreValue semaphore;
    void* address = &semaphore;
    REQUIRE(SUCCEEDED(semaphore.CreateFromPointer(L"test", address)));
    void* ptr;
    REQUIRE(SUCCEEDED(semaphore.TryGetPointer(L"test", &ptr)));
    REQUIRE(ptr == address);
}

TEST_CASE("ResultTests::ProcessLocalStorage", "[result]")
{
    // Test process local storage memory and ref-counting
    {
        wil::details_abi::ProcessLocalStorage<SharedObject> obj1("ver1");
        wil::details_abi::ProcessLocalStorage<SharedObject> obj2("ver1");

        auto& o1 = *obj1.GetShared();
        auto& o2 = *obj2.GetShared();

        REQUIRE(o1.value == 0);
        REQUIRE(o2.value == 0);
        o1.value = 42;
        REQUIRE(o2.value == 42);
        REQUIRE(objectCount == 1);

        wil::details_abi::ProcessLocalStorage<SharedObject> obj3("ver3");
        auto& o3 = *obj3.GetShared();

        REQUIRE(o3.value == 0);
        REQUIRE(objectCount == 2);
    }

    REQUIRE(objectCount == 0);
}

#ifdef WIL_ENABLE_EXCEPTIONS
#pragma warning(push)
#pragma warning(disable : 4702) // Unreachable code
TEST_CASE("ResultTests::ExceptionHandling", "[result]")
{
    witest::TestFailureCache failures;

    SECTION("Test 'what()' implementation on ResultException")
    {
        auto swap = witest::AssignTemporaryValue(&wil::g_fResultThrowPlatformException, false);
        try
        {
            THROW_HR(E_INVALIDARG);
            FAIL("Expected an exception");
        }
        catch (const std::exception& exception)
        {
            REQUIRE(failures.size() == 1);
            REQUIRE(failures[0].hr == E_INVALIDARG);
            auto what = exception.what();
            REQUIRE((what && *what));
            REQUIRE(strstr(what, "Exception") != nullptr);
        }
    }
    failures.clear();

    SECTION("Test messaging from an unhandled std exception")
    {
        // #pragma warning(suppress: 28931)  // unused assignment -- it IS being used... seems like a tool issue.
        auto hr = []() {
            try
            {
                throw std::runtime_error("runtime");
            }
            catch (...)
            {
                RETURN_CAUGHT_EXCEPTION();
            }
        }();
        REQUIRE(failures.size() == 1);
        REQUIRE(failures[0].hr == HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION));
        REQUIRE(wcsstr(failures[0].pszMessage, L"runtime") != nullptr); // should get the exception what() string...
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION));
    }
    failures.clear();

    SECTION("Test messaging from bad_alloc")
    {
        auto hr = []() -> HRESULT {
            try
            {
                throw std::bad_alloc();
            }
            catch (...)
            {
                RETURN_CAUGHT_EXCEPTION();
            }
        }();
        REQUIRE(failures.size() == 1);
        REQUIRE(failures[0].hr == E_OUTOFMEMORY);
        REQUIRE(wcsstr(failures[0].pszMessage, L"alloc") != nullptr); // should get the exception what() string...
        REQUIRE(hr == E_OUTOFMEMORY);
    }
    failures.clear();

    SECTION("Test messaging from a WIL exception")
    {
        auto hr = []() -> HRESULT {
            try
            {
                THROW_HR(E_INVALIDARG);
            }
            catch (...)
            {
                RETURN_CAUGHT_EXCEPTION();
            }
            return S_OK;
        }();
        REQUIRE(failures.size() == 2);
        REQUIRE(failures[0].hr == E_INVALIDARG);
        REQUIRE(failures[0].pszMessage == nullptr);
        REQUIRE(failures[1].hr == E_INVALIDARG);
        REQUIRE(wcsstr(failures[1].pszMessage, L"Exception") != nullptr); // should get the exception debug string...
        REQUIRE(hr == E_INVALIDARG);
    }
    failures.clear();

    SECTION("Fail fast an unknown exception")
    {
        REQUIRE(witest::DoesCodeFailFast([] {
            try
            {
                throw E_INVALIDARG; // bad throw... (long)
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }));
    }
    failures.clear();

    SECTION("Log test (returns hr)")
    {
        HRESULT hr = S_OK;
        try
        {
            throw std::bad_alloc();
        }
        catch (...)
        {
            hr = LOG_CAUGHT_EXCEPTION();
            auto hrDirect = wil::ResultFromCaughtException();
            REQUIRE(hr == hrDirect);
        }
        REQUIRE(failures.size() == 1);
        REQUIRE(failures[0].hr == E_OUTOFMEMORY);
        REQUIRE(wcsstr(failures[0].pszMessage, L"alloc") != nullptr); // should get the exception what() string...
        REQUIRE(hr == E_OUTOFMEMORY);
    }
    failures.clear();

    SECTION("Fail-fast test")
    {
        REQUIRE(witest::DoesCodeFailFast([] {
            try
            {
                throw std::bad_alloc();
            }
            catch (...)
            {
                FAIL_FAST_CAUGHT_EXCEPTION();
            }
        }));
        REQUIRE(failures.size() == 1);
        REQUIRE(failures[0].hr == E_OUTOFMEMORY);
        REQUIRE(wcsstr(failures[0].pszMessage, L"alloc") != nullptr); // should get the exception what() string...
    }
    failures.clear();

    SECTION("Exception test (different exception type thrown...)")
    {
        auto swap = witest::AssignTemporaryValue(&wil::g_fResultThrowPlatformException, false);
        size_t line = 0;
        try
        {
            try
            {
                throw std::bad_alloc();
            }
            catch (...)
            {
                // clang-format off
                line = __LINE__;  THROW_NORMALIZED_CAUGHT_EXCEPTION();
                // clang-format on
            }
        }
        catch (const wil::ResultException& exception)
        {
            REQUIRE(exception.GetFailureInfo().uLineNumber == line); // should have thrown new, so we should have the rethrow line number
            REQUIRE(exception.GetErrorCode() == E_OUTOFMEMORY);
        }
        catch (...)
        {
            FAIL();
        }
        REQUIRE(failures.size() == 1);
        REQUIRE(failures[0].hr == E_OUTOFMEMORY);
        REQUIRE(wcsstr(failures[0].pszMessage, L"alloc") != nullptr); // should get the exception what() string...
    }
    failures.clear();

    SECTION("Exception test (rethrow same exception type...)")
    {
        auto swap = witest::AssignTemporaryValue(&wil::g_fResultThrowPlatformException, false);
        size_t line = 0;
        try
        {
            try
            {
                // clang-format off
                line = __LINE__;  THROW_HR(E_OUTOFMEMORY);
                // clang-format on
            }
            catch (...)
            {
                THROW_NORMALIZED_CAUGHT_EXCEPTION();
            }
        }
        catch (const wil::ResultException& exception)
        {
            REQUIRE(exception.GetFailureInfo().uLineNumber == line); // should have re-thrown the original exception (with the original line number)
        }
        catch (...)
        {
            FAIL();
        }
    }
    failures.clear();

    SECTION("Test catch message")
    {
        try
        {
            throw std::bad_alloc();
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION_MSG("train: %d", 42);
        }
        REQUIRE(failures.size() == 1);
        REQUIRE(failures[0].hr == E_OUTOFMEMORY);
        REQUIRE(wcsstr(failures[0].pszMessage, L"alloc") != nullptr); // should get the exception what() string...
        REQUIRE(wcsstr(failures[0].pszMessage, L"train") != nullptr); // should *also* get the message...
        REQUIRE(wcsstr(failures[0].pszMessage, L"42") != nullptr);
    }
    failures.clear();

    SECTION("Test messaging from a WIL exception")
    {
        auto hr = []() -> HRESULT {
            try
            {
                throw std::bad_alloc();
            }
            catch (...)
            {
                RETURN_CAUGHT_EXCEPTION_EXPECTED();
            }
        }();
        REQUIRE(failures.empty());
        REQUIRE(hr == E_OUTOFMEMORY);
    }
    failures.clear();

    SECTION("Test ResultFromException...")
    {
        auto hrOk = wil::ResultFromException([&] {});
        REQUIRE(hrOk == S_OK);

        auto hr = wil::ResultFromException([&] {
            throw std::bad_alloc();
        });
        REQUIRE(failures.empty());
        REQUIRE(hr == E_OUTOFMEMORY);
    }
    failures.clear();

    SECTION("Explicit failfast for unrecognized")
    {
        REQUIRE(witest::DoesCodeFailFast([] {
            wil::ResultFromException([&] {
                throw E_FAIL;
            });
        }));
        REQUIRE(failures.size() == 1);
        REQUIRE(failures[0].hr == HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION));
    }
    failures.clear();

    SECTION("Manual debug-only validation of the SEH failfast")
    {
        auto hr1 = wil::ResultFromExceptionDebug(WI_DIAGNOSTICS_INFO, [&]() {
            // Uncomment to test SEH fail-fast
            // throw E_FAIL;
        });
        REQUIRE(hr1 == S_OK);

        auto hr2 = wil::ResultFromExceptionDebug(WI_DIAGNOSTICS_INFO, wil::SupportedExceptions::Thrown, [&] {
            // Uncomment to test SEH fail-fast
            // throw std::range_error("range");
        });
        REQUIRE(hr2 == S_OK);

        wil::FailFastException(WI_DIAGNOSTICS_INFO, [&] {
            // Uncomment to test SEH fail-fast
            // THROW_HR(E_FAIL);
        });
    }
    failures.clear();

    SECTION("Standard")
    {
        // clang-format off
        auto line = __LINE__;  auto hr = wil::ResultFromExceptionDebug(WI_DIAGNOSTICS_INFO, [&] {
            THROW_HR(E_INVALIDARG);
        });
        // clang-format on
        REQUIRE(failures.size() == 2);
        REQUIRE(static_cast<decltype(line)>(failures[1].uLineNumber) == line);
        REQUIRE(hr == E_INVALIDARG);
    }
    failures.clear();

    SECTION("bad_alloc")
    {
        auto hr = wil::ResultFromExceptionDebug(WI_DIAGNOSTICS_INFO, [&] {
            throw std::bad_alloc();
        });
        REQUIRE(failures.size() == 1);
        REQUIRE(hr == E_OUTOFMEMORY);
    }
    failures.clear();

    SECTION("std::exception")
    {
        auto hr = wil::ResultFromExceptionDebug(WI_DIAGNOSTICS_INFO, [&] {
            throw std::range_error("range");
        });
        REQUIRE(failures.size() == 1);
        REQUIRE(wcsstr(failures[0].pszMessage, L"range") != nullptr);
        REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION));
    }
}

void ExceptionHandlingCompilationTest()
{
    [] {
        try
        {
            throw std::bad_alloc();
        }
        CATCH_RETURN();
    }();
    [] {
        try
        {
            throw std::bad_alloc();
        }
        CATCH_RETURN_MSG("train: %d", 42);
    }();
    [] {
        try
        {
            throw std::bad_alloc();
        }
        CATCH_RETURN_EXPECTED();
    }();
    [] {
        try
        {
            throw std::bad_alloc();
        }
        catch (...)
        {
            RETURN_CAUGHT_EXCEPTION();
        }
    }();
    [] {
        try
        {
            throw std::bad_alloc();
        }
        catch (...)
        {
            RETURN_CAUGHT_EXCEPTION_MSG("train: %d", 42);
        }
    }();
    [] {
        try
        {
            throw std::bad_alloc();
        }
        catch (...)
        {
            RETURN_CAUGHT_EXCEPTION_EXPECTED();
        }
    }();

    try
    {
        throw std::bad_alloc();
    }
    CATCH_LOG();
    try
    {
        throw std::bad_alloc();
    }
    CATCH_LOG_MSG("train: %d", 42);
    try
    {
        throw std::bad_alloc();
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }
    try
    {
        throw std::bad_alloc();
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION_MSG("train: %d", 42);
    }

    try
    {
        throw std::bad_alloc();
    }
    CATCH_FAIL_FAST();
    try
    {
        throw std::bad_alloc();
    }
    CATCH_FAIL_FAST_MSG("train: %d", 42);
    try
    {
        throw std::bad_alloc();
    }
    catch (...)
    {
        FAIL_FAST_CAUGHT_EXCEPTION();
    }
    try
    {
        throw std::bad_alloc();
    }
    catch (...)
    {
        FAIL_FAST_CAUGHT_EXCEPTION_MSG("train: %d", 42);
    }

    try
    {
        try
        {
            throw std::bad_alloc();
        }
        CATCH_THROW_NORMALIZED();
    }
    catch (...)
    {
    }
    try
    {
        try
        {
            throw std::bad_alloc();
        }
        CATCH_THROW_NORMALIZED_MSG("train: %d", 42);
    }
    catch (...)
    {
    }
    try
    {
        try
        {
            throw std::bad_alloc();
        }
        catch (...)
        {
            THROW_NORMALIZED_CAUGHT_EXCEPTION();
        }
    }
    catch (...)
    {
    }
    try
    {
        try
        {
            throw std::bad_alloc();
        }
        catch (...)
        {
            THROW_NORMALIZED_CAUGHT_EXCEPTION_MSG("train: %d", 42);
        }
    }
    catch (...)
    {
    }

    wil::ResultFromExceptionDebug(WI_DIAGNOSTICS_INFO, wil::SupportedExceptions::All, [&] {
        THROW_HR(E_FAIL);
    });

    wil::ResultFromException(WI_DIAGNOSTICS_INFO, wil::SupportedExceptions::None, [&] {});

    wil::ResultFromException([&] {});

    wil::FailFastException(WI_DIAGNOSTICS_INFO, [&] {});
}
#pragma warning(pop)
#endif

TEST_CASE("ResultTests::ErrorMacros", "[result]")
{
    REQUIRE_ERROR(FAIL_FAST());
    REQUIRE_ERROR(FAIL_FAST_IF(true));
    REQUIRE_ERROR(FAIL_FAST_IF_NULL(nullptr));

    REQUIRE_NOERROR(FAIL_FAST_IF(false));
    REQUIRE_NOERROR(FAIL_FAST_IF_NULL(_ReturnAddress()));

    REQUIRE_ERROR(FAIL_FAST_MSG("%d", 42));
    REQUIRE_ERROR(FAIL_FAST_IF_MSG(true, "%d", 42));
    REQUIRE_ERROR(FAIL_FAST_IF_NULL_MSG(nullptr, "%d", 42));

    REQUIRE_NOERROR(FAIL_FAST_IF_MSG(false, "%d", 42));
    REQUIRE_NOERROR(FAIL_FAST_IF_NULL_MSG(_ReturnAddress(), "%d", 42));

    // wil::g_pfnResultLoggingCallback = ResultMacrosLoggingCallback;
    SetLastError(ERROR_PRINTER_ALREADY_EXISTS);
    REQUIRE_ERROR(__FAIL_FAST_ASSERT_WIN32_BOOL_FALSE__(FALSE));
    REQUIRE_NOERROR(__FAIL_FAST_ASSERT_WIN32_BOOL_FALSE__(TRUE));
}

// The originate helper isn't compatible with CX so don't test it in that mode.
#if !defined(__cplusplus_winrt) && (NTDDI_VERSION >= NTDDI_WIN8)
TEST_CASE("ResultTests::NoOriginationByDefault", "[result]")
{
    ::wil::SetOriginateErrorCallback(nullptr);
    wil::com_ptr_nothrow<IRestrictedErrorInfo> restrictedErrorInformation;

    // We can't guarantee test order, so clear the error payload prior to starting
    SetRestrictedErrorInfo(nullptr);

    []() -> HRESULT {
        RETURN_HR(S_OK);
    }();
    REQUIRE(S_FALSE == GetRestrictedErrorInfo(&restrictedErrorInformation));

#ifdef WIL_ENABLE_EXCEPTIONS
    try
    {
        THROW_HR(E_FAIL);
    }
    catch (...)
    {
    }
    REQUIRE(S_FALSE == GetRestrictedErrorInfo(&restrictedErrorInformation));
#endif // WIL_ENABLE_EXCEPTIONS

    []() -> HRESULT {
        RETURN_HR(E_FAIL);
    }();
    REQUIRE(S_FALSE == GetRestrictedErrorInfo(&restrictedErrorInformation));

    []() -> HRESULT {
        RETURN_IF_FAILED_EXPECTED(E_ACCESSDENIED);
        return S_OK;
    }();
    REQUIRE(S_FALSE == GetRestrictedErrorInfo(&restrictedErrorInformation));
}

TEST_CASE("ResultTests::AutomaticOriginationOnFailure", "[result]")
{
    ::wil::SetOriginateErrorCallback(::wil::details::RaiseRoOriginateOnWilExceptions);
    wil::com_ptr_nothrow<IRestrictedErrorInfo> restrictedErrorInformation;

    // Make sure we don't start with an error payload
    SetRestrictedErrorInfo(nullptr);

    // Success codes shouldn't originate.
    []() {
        RETURN_HR(S_OK);
    }();
    REQUIRE(S_FALSE == GetRestrictedErrorInfo(&restrictedErrorInformation));

    auto validateOriginatedError = [&](HRESULT hrExpected) {
        wil::unique_bstr descriptionUnused;
        HRESULT existingHr = S_OK;
        wil::unique_bstr restrictedDescriptionUnused;
        wil::unique_bstr capabilitySidUnused;
        REQUIRE_SUCCEEDED(restrictedErrorInformation->GetErrorDetails(
            &descriptionUnused, &existingHr, &restrictedDescriptionUnused, &capabilitySidUnused));
        REQUIRE(hrExpected == existingHr);
    };

#ifdef WIL_ENABLE_EXCEPTIONS
    // Throwing an error should originate.
    constexpr HRESULT thrownErrorCode = TYPE_E_ELEMENTNOTFOUND;
    try
    {
        THROW_HR(thrownErrorCode);
    }
    catch (...)
    {
    }
    REQUIRE(S_OK == GetRestrictedErrorInfo(&restrictedErrorInformation));
    validateOriginatedError(thrownErrorCode);
#endif // WIL_ENABLE_EXCEPTIONS

    // Returning an error code should originate.
    static constexpr HRESULT returnedErrorCode = REGDB_E_CLASSNOTREG;
    []() {
        RETURN_HR(returnedErrorCode);
    }();
    REQUIRE(S_OK == GetRestrictedErrorInfo(&restrictedErrorInformation));
    validateOriginatedError(returnedErrorCode);

    // _EXPECTED errors should NOT originate.
    static constexpr HRESULT expectedErrorCode = E_ACCESSDENIED;
    []() {
        RETURN_IF_FAILED_EXPECTED(expectedErrorCode);
        return S_OK;
    }();
    REQUIRE(S_FALSE == GetRestrictedErrorInfo(&restrictedErrorInformation));
}

TEST_CASE("ResultTests::OriginatedWithMessagePreserved", "[result]")
{
    SetRestrictedErrorInfo(nullptr);

#ifdef WIL_ENABLE_EXCEPTIONS
    try
    {
        THROW_HR_MSG(E_FAIL, "Puppies not allowed");
    }
    catch (...)
    {
    }
    witest::RequireRestrictedErrorInfo(E_FAIL, L"Puppies not allowed");

    []() {
        try
        {
            throw std::exception("Puppies not allowed");
        }
        CATCH_RETURN();
    }();
    witest::RequireRestrictedErrorInfo(HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION), L"std::exception: Puppies not allowed");

#endif

    []() {
        RETURN_HR_MSG(E_FAIL, "Puppies not allowed");
    }();
    witest::RequireRestrictedErrorInfo(E_FAIL, L"Puppies not allowed");
}

#endif

static void __stdcall CustomLoggingCallback(const wil::FailureInfo&) noexcept
{
    ::SetLastError(ERROR_ABANDON_HIBERFILE);
}

TEST_CASE("ResultTests::ReportDoesNotChangeLastError", "[result]")
{
    decltype(wil::details::g_pfnLoggingCallback) oopsie = CustomLoggingCallback;
    auto swap = witest::AssignTemporaryValue(&wil::details::g_pfnLoggingCallback, oopsie);

    ::SetLastError(ERROR_ABIOS_ERROR);
    LOG_IF_WIN32_BOOL_FALSE(FALSE);
    REQUIRE(::GetLastError() == ERROR_ABIOS_ERROR);
}

struct basic_error_policy
{
    // Only has the required things
    struct result
    {
        HRESULT hr;
    };

    static result HResult(HRESULT hr)
    {
        return result{ hr };
    }
};

struct custom_error_policy
{
    // Has all required and optional things
    using result = int;

    static constexpr const bool is_nothrow = true;

    static result HResult(HRESULT) noexcept
    {
        return 0;
    }

    static result OK() noexcept
    {
        return 1;
    }

    static result Win32Error(DWORD) noexcept
    {
        return 2;
    }

    static result LastError() noexcept
    {
        return 3;
    }

    static result Win32BOOL(BOOL) noexcept
    {
        return 4;
    }

    static result Win32Handle(HANDLE, HANDLE*) noexcept
    {
        return 5;
    }

    static result LastErrorIfFalse(bool) noexcept
    {
        return 6;
    }

    template <typename T>
    static result Pointer(T*) noexcept
    {
        return 7;
    }
};

TEST_CASE("ResultTests::ErrorPolicyTraits", "[result]")
{
    using BasicTraits = wil::err_policy_traits<basic_error_policy>;
    REQUIRE(!BasicTraits::is_nothrow);
    REQUIRE(BasicTraits::HResult(E_FAIL).hr == E_FAIL);
    REQUIRE(BasicTraits::OK().hr == S_OK);
    REQUIRE(BasicTraits::Win32Error(ERROR_ACCESS_DENIED).hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));
    ::SetLastError(ERROR_FILE_NOT_FOUND);
    REQUIRE(BasicTraits::LastError().hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    REQUIRE(BasicTraits::Win32BOOL(TRUE).hr == S_OK);
    ::SetLastError(ERROR_FILE_EXISTS);
    REQUIRE(BasicTraits::Win32BOOL(FALSE).hr == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));
    HANDLE result;
    REQUIRE(BasicTraits::Win32Handle(reinterpret_cast<HANDLE>(42), &result).hr == S_OK);
    REQUIRE(result == reinterpret_cast<HANDLE>(42));
    ::SetLastError(ERROR_BAD_DEVICE);
    REQUIRE(BasicTraits::Win32Handle(nullptr, &result).hr == HRESULT_FROM_WIN32(ERROR_BAD_DEVICE));
    REQUIRE(result == nullptr);
    REQUIRE(BasicTraits::LastErrorIfFalse(true).hr == S_OK);
    ::SetLastError(ERROR_FILE_ENCRYPTED);
    REQUIRE(BasicTraits::LastErrorIfFalse(false).hr == HRESULT_FROM_WIN32(ERROR_FILE_ENCRYPTED));
    REQUIRE(BasicTraits::Pointer(&result).hr == S_OK);
    REQUIRE(BasicTraits::Pointer(static_cast<int*>(nullptr)).hr == E_OUTOFMEMORY);

    using CustomTraits = wil::err_policy_traits<custom_error_policy>;
    REQUIRE(CustomTraits::is_nothrow);
    REQUIRE(CustomTraits::HResult(E_FAIL) == 0);
    REQUIRE(CustomTraits::OK() == 1);
    REQUIRE(CustomTraits::Win32Error(ERROR_ACCESS_DENIED) == 2);
    REQUIRE(CustomTraits::LastError() == 3);
    REQUIRE(CustomTraits::Win32BOOL(FALSE) == 4);
    REQUIRE(CustomTraits::Win32Handle(nullptr, nullptr) == 5);
    REQUIRE(CustomTraits::LastErrorIfFalse(false) == 6);
    REQUIRE(CustomTraits::Pointer(&result) == 7);
}
