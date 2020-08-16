
#include <wil/resource.h>
#include <wil/Safearrays.h>
#include <wil/com.h>

#include "common.h"
#include <vector>

#define RUN_TEST_TYPED_NOTHROW(test)        WI_FOREACH(test                                 \
                                               , wil::unique_char_safearray_nothrow         \
                                               , wil::unique_short_safearray_nothrow        \
                                               , wil::unique_long_safearray_nothrow         \
                                               , wil::unique_int_safearray_nothrow          \
                                               , wil::unique_longlong_safearray_nothrow     \
                                               , wil::unique_byte_safearray_nothrow         \
                                               , wil::unique_word_safearray_nothrow         \
                                               , wil::unique_dword_safearray_nothrow        \
                                               , wil::unique_ulonglong_safearray_nothrow    \
                                               , wil::unique_float_safearray_nothrow        \
                                               , wil::unique_double_safearray_nothrow       \
                                               , wil::unique_varbool_safearray_nothrow      \
                                               , wil::unique_date_safearray_nothrow         \
                                               , wil::unique_currency_safearray_nothrow     \
                                               , wil::unique_decimal_safearray_nothrow      \
                                               , wil::unique_bstr_safearray_nothrow         \
                                               , wil::unique_unknown_safearray_nothrow      \
                                               , wil::unique_dispatch_safearray_nothrow     \
                                               , wil::unique_variant_safearray_nothrow      \
                                               )

#define RUN_TEST_TYPED_FAILFAST(test)        WI_FOREACH(test                                \
                                               , wil::unique_char_safearray_failfast        \
                                               , wil::unique_short_safearray_failfast       \
                                               , wil::unique_long_safearray_failfast        \
                                               , wil::unique_int_safearray_failfast         \
                                               , wil::unique_longlong_safearray_failfast    \
                                               , wil::unique_byte_safearray_failfast        \
                                               , wil::unique_word_safearray_failfast        \
                                               , wil::unique_dword_safearray_failfast       \
                                               , wil::unique_ulonglong_safearray_failfast   \
                                               , wil::unique_float_safearray_failfast       \
                                               , wil::unique_double_safearray_failfast      \
                                               , wil::unique_varbool_safearray_failfast     \
                                               , wil::unique_date_safearray_failfast        \
                                               , wil::unique_currency_safearray_failfast    \
                                               , wil::unique_decimal_safearray_failfast     \
                                               , wil::unique_bstr_safearray_failfast        \
                                               , wil::unique_unknown_safearray_failfast     \
                                               , wil::unique_dispatch_safearray_failfast    \
                                               , wil::unique_variant_safearray_failfast     \
                                               )

#define RUN_TEST_TYPED(test)                WI_FOREACH(test                                 \
                                               , wil::unique_char_safearray                 \
                                               , wil::unique_short_safearray                \
                                               , wil::unique_long_safearray                 \
                                               , wil::unique_int_safearray                  \
                                               , wil::unique_longlong_safearray             \
                                               , wil::unique_byte_safearray                 \
                                               , wil::unique_word_safearray                 \
                                               , wil::unique_dword_safearray                \
                                               , wil::unique_ulonglong_safearray            \
                                               , wil::unique_float_safearray                \
                                               , wil::unique_double_safearray               \
                                               , wil::unique_varbool_safearray              \
                                               , wil::unique_date_safearray                 \
                                               , wil::unique_currency_safearray             \
                                               , wil::unique_decimal_safearray              \
                                               , wil::unique_bstr_safearray                 \
                                               , wil::unique_unknown_safearray              \
                                               , wil::unique_dispatch_safearray             \
                                               , wil::unique_variant_safearray              \
                                               )

template<typename T, typename = typename wistd::enable_if<wistd::is_integral<T>::value, int>::type>
std::vector<T> GetSampleData()
{
    constexpr auto BIT_COUNT = sizeof(T) * 8;

    auto result = std::vector<T>{};
    result.reserve(BIT_COUNT);
    for (auto i = 0; i < BIT_COUNT; ++i)
    {
        result.emplace_back(1 << i);
    }
    return result;
}

template<typename T, typename = typename wistd::enable_if<wistd::is_floating_point<T>::value, int>::type>
std::vector<T> GetSampleData()
{
    auto result = std::vector<T>{};
    result.reserve(32);
    for (auto i = 1; i <= 16; ++i)
    {
        result.emplace_back(1 / i);
        result.emplace_back(2 * i);
    }
    return result;
}

template<typename T, typename = typename wistd::enable_if<wistd::is_same<T, BSTR>::value, int>::type>
std::vector<wil::unique_bstr> GetSampleData()
{
    auto result = std::vector<wil::unique_bstr>{};
    result.reserve(32);
    for (auto i = 0; i < 32; ++i)
    {
        wil::unique_bstr temp{};
        switch (i % 4)
        {
        case 0:
            temp.reset(::SysAllocString(L"Sample Data"));
            break;
        case 1:
            temp.reset(::SysAllocString(L"Larger Sample Data"));
            break;
        case 2:
            temp.reset(::SysAllocString(L"This is much much larger Sample Data"));
            break;
        case 3:
            temp.reset(::SysAllocString(L"This is the longest Sample Data.  It's the longest by a lot.  I mean a lot."));
            break;
        }
        result.emplace_back(std::move(temp));
    }
    return result;
}

template<typename T, typename = typename wistd::enable_if<wistd::is_same<T, VARIANT>::value, int>::type>
std::vector<wil::unique_variant> GetSampleData()
{
    auto result = std::vector<wil::unique_variant>{};
    result.reserve(32);
    for (auto i = 0; i < 32; ++i)
    {
        result.push_back({});
        auto& var = result.back();
        switch (i % 4)
        {
        case 0:
            V_VT(&var) = VT_I4;
            V_I4(&var) = 37;
            break;
        case 1:
            V_VT(&var) = VT_I1;
            V_I1(&var) = 0x40;
            break;
        case 2:
            V_VT(&var) = VT_UI2;
            V_UI2(&var) = 0x4000;
            break;
        case 3:
            V_VT(&var) = VT_R4;
            V_R4(&var) = 98.6f;
            break;
        }
    }
    return result;
}

template<typename T, typename = typename wistd::enable_if<wistd::is_same<T, LPUNKNOWN>::value, int>::type>
std::vector<wil::com_ptr<IUnknown>> GetSampleData() { return {} };

template<typename T, typename = typename wistd::enable_if<wistd::is_same<T, LPDISPATCH>::value, int>::type>
std::vector<wil::com_ptr<IDispatch>> GetSampleData() { return {} };

template<typename safearray_t>
void TestLock(safearray_t& sa)
{
    REQUIRE(sa);
    const auto startingLocks = sa.get()->cLocks;
    {
        auto lock = sa.scope_lock();
        REQUIRE(sa.get()->cLocks > startingLocks); // Verify Lock Count increased
    }
    REQUIRE(startingLocks == sa.get()->cLocks);   // Verify it dropped back down
}

template<typename safearray_t>
void TestTyped_Create_NoThrow()
{
    constexpr auto SIZE = 256U;

    auto sa = safearray_t{};
    LONG val = 0;
    ULONG count = 0;
    REQUIRE_SUCCEEDED(sa.create(SIZE));
    REQUIRE(sa);
    REQUIRE(sa.dim() == 1);
    REQUIRE(sa.elemsize() == sizeof(safearray_t::elemtype));
    REQUIRE_SUCCEEDED(sa.count(&count));
    REQUIRE(count == SIZE);
    REQUIRE_SUCCEEDED(sa.lbound(&val));
    REQUIRE(val == 0);
    REQUIRE_SUCCEEDED(sa.ubound(&val));
    REQUIRE(val == SIZE - 1);
    TestLock<safearray_t>(sa);
    sa.reset();
    REQUIRE(!sa);
}

template<typename safearray_t>
void TestTyped_Create_FailFast()
{
    constexpr auto SIZE = 256U;

    auto sa = safearray_t{};
    LONG val = 0;
    ULONG count = 0;
    REQUIRE_NOCRASH(sa.create(SIZE));
    REQUIRE(sa);
    REQUIRE(sa.dim() == 1);
    REQUIRE(sa.elemsize() == sizeof(safearray_t::elemtype));
    REQUIRE_NOCRASH(sa.count(&count));
    REQUIRE(count == SIZE);
    REQUIRE_NOCRASH(sa.lbound(&val));
    REQUIRE(val == 0);
    REQUIRE_NOCRASH(sa.ubound(&val));
    REQUIRE(val == SIZE - 1);
    TestLock<safearray_t>(sa);
    sa.reset();
    REQUIRE(!sa);
}

#ifdef WIL_ENABLE_EXCEPTIONS
template<typename safearray_t>
void TestTyped_Create()
{
    constexpr auto SIZE = 256U;

    auto sa = safearray_t{};
    REQUIRE_NOTHROW(sa = safearray_t{ SIZE });
    REQUIRE(sa);
    REQUIRE(sa.dim() == 1);
    REQUIRE(sa.elemsize() == sizeof(safearray_t::elemtype));
    REQUIRE_NOTHROW(sa.count() == SIZE);
    REQUIRE_NOTHROW(sa.lbound() == 0);
    REQUIRE_NOTHROW(sa.ubound() == SIZE-1);
    TestLock<safearray_t>(sa);
    sa.reset();
    REQUIRE(!sa);
}
#endif

#define _TYPED_CREATE_NOTHROW(type)         TestTyped_Create_NoThrow<type>();
#define _TYPED_CREATE_FAILFAST(type)        TestTyped_Create_FailFast<type>();
#define _TYPED_CREATE(type)                 TestTyped_Create<type>();

template<typename safearray_t>
void TestTyped_DirectElementAccess_NoThrow()
{
    constexpr auto SIZE = 256U;

    const auto sample_data = GetSampleData<safearray_t::elemtype>();
    auto sa = safearray_t{};
    LONG val = 0;
    ULONG count = 0;
    REQUIRE_SUCCEEDED(sa.create(SIZE));
    REQUIRE(sa);
    REQUIRE(sa.dim() == 1);
    REQUIRE(sa.elemsize() == sizeof(safearray_t::elemtype));
    REQUIRE_SUCCEEDED(sa.count(&count));
    REQUIRE(count == SIZE);
    REQUIRE_SUCCEEDED(sa.lbound(&val));
    REQUIRE(val == 0);
    REQUIRE_SUCCEEDED(sa.ubound(&val));
    REQUIRE(val == SIZE - 1);
    TestLock<safearray_t>(sa);
    sa.reset();
    REQUIRE(!sa);
}




TEST_CASE("SafearrayTests::Create", "[safearray][create]")
{
    SECTION("Create SafeArray - No Throw")
    {
        RUN_TEST_TYPED_NOTHROW(_TYPED_CREATE_NOTHROW);
    }

    SECTION("Create SafeArray - FailFast")
    {
        RUN_TEST_TYPED_FAILFAST(_TYPED_CREATE_FAILFAST);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Create SafeArray - Exceptions")
    {
        RUN_TEST_TYPED(_TYPED_CREATE);
    }
#endif
}

TEST_CASE("SafearrayTests::AccessData", "[safearray][data]")
{
    SECTION("Basic")
    {
        constexpr auto SIZE = 32;

        wil::unique_safearray_nothrow sa;

        REQUIRE_SUCCEEDED(sa.create(VT_UI4, SIZE));

        for (auto i = 0; i < SIZE; ++i)
        {
            sa.put_element((1 << i), i);
        }

        {
            wil::unique_safearray_nothrow::unique_accessdata_t<UINT> data;
            UINT counter = 0;
            REQUIRE_SUCCEEDED(sa.access_data(data));
            for (auto& n : data)
            {
                REQUIRE(n == static_cast<UINT>(1 << counter++));
            }
        }
    }

}
