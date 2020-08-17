#include <wil/resource.h>
#include <wil/Safearrays.h>
#include <wil/com.h>

#include "common.h"
#include <vector>
#include <array>
#include <propvarutil.h>

#pragma comment( lib, "Propsys.lib" )

#define RUN_TYPED_TEST_NOTHROW(test)        WI_FOREACH(test                                 \
                                                , wil::unique_char_safearray_nothrow        \
                                                , wil::unique_long_safearray_nothrow        \
                                                , wil::unique_int_safearray_nothrow         \
                                                , wil::unique_longlong_safearray_nothrow    \
                                                , wil::unique_byte_safearray_nothrow        \
                                                , wil::unique_word_safearray_nothrow        \
                                                , wil::unique_dword_safearray_nothrow       \
                                                , wil::unique_ulonglong_safearray_nothrow   \
                                                , wil::unique_float_safearray_nothrow       \
                                                , wil::unique_varbool_safearray_nothrow     \
                                                , wil::unique_date_safearray_nothrow        \
                                                , wil::unique_bstr_safearray_nothrow        \
                                                , wil::unique_unknown_safearray_nothrow     \
                                                , wil::unique_dispatch_safearray_nothrow    \
                                                , wil::unique_variant_safearray_nothrow     \
                                               )

#define RUN_TYPED_TEST_FAILFAST(test)        WI_FOREACH(test                                \
                                                , wil::unique_char_safearray_failfast       \
                                                , wil::unique_long_safearray_failfast       \
                                                , wil::unique_int_safearray_failfast        \
                                                , wil::unique_longlong_safearray_failfast   \
                                                , wil::unique_byte_safearray_failfast       \
                                                , wil::unique_word_safearray_failfast       \
                                                , wil::unique_dword_safearray_failfast      \
                                                , wil::unique_ulonglong_safearray_failfast  \
                                                , wil::unique_float_safearray_failfast      \
                                                , wil::unique_varbool_safearray_failfast    \
                                                , wil::unique_date_safearray_failfast       \
                                                , wil::unique_bstr_safearray_failfast       \
                                                , wil::unique_unknown_safearray_failfast    \
                                                , wil::unique_dispatch_safearray_failfast   \
                                                , wil::unique_variant_safearray_failfast    \
                                            )

#define RUN_TYPED_TEST(test)                WI_FOREACH(test                                 \
                                                , wil::unique_char_safearray                \
                                                , wil::unique_long_safearray                \
                                                , wil::unique_int_safearray                 \
                                                , wil::unique_longlong_safearray            \
                                                , wil::unique_byte_safearray                \
                                                , wil::unique_word_safearray                \
                                                , wil::unique_dword_safearray               \
                                                , wil::unique_ulonglong_safearray           \
                                                , wil::unique_float_safearray               \
                                                , wil::unique_varbool_safearray             \
                                                , wil::unique_date_safearray                \
                                                , wil::unique_bstr_safearray                \
                                                , wil::unique_unknown_safearray             \
                                                , wil::unique_dispatch_safearray            \
                                                , wil::unique_variant_safearray             \
                                            )

#define RUN_TEST(test)                      WI_FOREACH(test                                 \
                                                , char                                      \
                                                , short                                     \
                                                , long                                      \
                                                , int                                       \
                                                , long long                                 \
                                                , unsigned char                             \
                                                , unsigned short                            \
                                                , unsigned long                             \
                                                , unsigned int                              \
                                                , unsigned long long                        \
                                                , float                                     \
                                                , double                                    \
                                                , VARIANT_BOOL                              \
                                                , DATE                                      \
                                                , BSTR                                      \
                                                , LPUNKNOWN                                 \
                                                , LPDISPATCH                                \
                                                , VARIANT                                   \
                                            )

constexpr auto DEFAULT_SAMPLE_SIZE = 32;

template<typename T, typename = typename wistd::enable_if<wistd::is_integral<T>::value, int>::type>
auto GetSampleData() -> std::array<T, sizeof(T)*8>
{
    constexpr auto BIT_COUNT = sizeof(T) * 8;

    auto result = std::array<T, BIT_COUNT>{};
    for (auto i = 0; i < BIT_COUNT; ++i)
    {
        result[i] = (static_cast<T>(1) << i);
    }
    return result;
}

template<typename T, typename = typename wistd::enable_if<wistd::is_floating_point<T>::value, int>::type>
std::array<T, DEFAULT_SAMPLE_SIZE> GetSampleData()
{
    auto result = std::array<T, DEFAULT_SAMPLE_SIZE>{};
    for (auto i = 0; i < DEFAULT_SAMPLE_SIZE/2; ++i)
    {
        result[i] = (i != 0) ? static_cast<T>(1 / i) : 0;
        result[i + (DEFAULT_SAMPLE_SIZE/2)] = 2* static_cast<T>(i);
    }
    return result;
}

template<typename T, typename = typename wistd::enable_if<wistd::is_same<T, BSTR>::value, int>::type>
std::array<wil::unique_bstr, DEFAULT_SAMPLE_SIZE> GetSampleData()
{
    auto result = std::array<wil::unique_bstr, DEFAULT_SAMPLE_SIZE>{};
    for (auto i = 0; i < DEFAULT_SAMPLE_SIZE; ++i)
    {
        switch (i % 4)
        {
        case 0:
            result[i].reset(::SysAllocString(L"Sample Data"));
            break;
        case 1:
            result[i].reset(::SysAllocString(L"Larger Sample Data"));
            break;
        case 2:
            result[i].reset(::SysAllocString(L"This is much much larger Sample Data"));
            break;
        case 3:
            result[i].reset(::SysAllocString(L"This is the longest Sample Data.  It's the longest by a lot.  I mean a lot."));
            break;
        }
    }
    return result;
}

template<typename T, typename = typename wistd::enable_if<wistd::is_same<T, VARIANT>::value, int>::type>
std::array<wil::unique_variant, DEFAULT_SAMPLE_SIZE> GetSampleData()
{
    auto result = std::array<wil::unique_variant, DEFAULT_SAMPLE_SIZE>{};
    for (auto i = 0; i < DEFAULT_SAMPLE_SIZE; ++i)
    {
        auto& var = result[i];
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
            V_VT(&var) = VT_BSTR;
            V_BSTR(&var) = ::SysAllocString(L"String in a variant");
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
std::array<wil::com_ptr<IUnknown>, 1> GetSampleData() { return {}; }

template<typename T, typename = typename wistd::enable_if<wistd::is_same<T, LPDISPATCH>::value, int>::type>
std::array<wil::com_ptr<IDispatch>, 1> GetSampleData() { return {}; }

template<typename T>
bool PerformCompare(const T& left, const T& right)
{
    return (left == right);
}

template<>
bool PerformCompare(const wil::unique_variant& left, const wil::unique_variant& right)
{
    return (::VariantCompare(left, right) == 0);
}

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

template<typename T>
void Test_Create_NoThrow()
{
    constexpr auto SIZE = 256U;

    auto sa = wil::unique_safearray_nothrow{};
    LONG val = 0;
    ULONG count = 0;
    //REQUIRE_SUCCEEDED(sa.create<T>(SIZE));
    REQUIRE(sa);
    REQUIRE(sa.dim() == 1);
    REQUIRE(sa.elemsize() == sizeof(T));
    REQUIRE_SUCCEEDED(sa.count(&count));
    REQUIRE(count == SIZE);
    REQUIRE_SUCCEEDED(sa.lbound(&val));
    REQUIRE(val == 0);
    REQUIRE_SUCCEEDED(sa.ubound(&val));
    REQUIRE(val == SIZE - 1);
    TestLock(sa);
    sa.reset();
    REQUIRE(!sa);
}

template<typename T>
void Test_Create_FailFast()
{
    constexpr auto SIZE = 256U;

    auto sa = wil::unique_safearray_failfast{};
    LONG val = 0;
    ULONG count = 0;
    REQUIRE_NOCRASH(sa.create<T>(SIZE));
    REQUIRE(sa);
    REQUIRE(sa.dim() == 1);
    REQUIRE(sa.elemsize() == sizeof(T));
    REQUIRE_NOCRASH(sa.count(&count));
    REQUIRE(count == SIZE);
    REQUIRE_NOCRASH(sa.lbound(&val));
    REQUIRE(val == 0);
    REQUIRE_NOCRASH(sa.ubound(&val));
    REQUIRE(val == SIZE - 1);
    TestLock(sa);
    sa.reset();
    REQUIRE(!sa);
}

#ifdef WIL_ENABLE_EXCEPTIONS
template<typename T>
void Test_Create()
{
    constexpr auto SIZE = 256U;

    wil::unique_safearray   sa{};
    REQUIRE_NOTHROW(sa.create<T>( SIZE ));
    REQUIRE(sa);
    REQUIRE(sa.dim() == 1);
    REQUIRE(sa.elemsize() == sizeof(T));
    REQUIRE_NOTHROW(sa.count() == SIZE);
    REQUIRE_NOTHROW(sa.lbound() == 0);
    REQUIRE_NOTHROW(sa.ubound() == SIZE - 1);
    TestLock(sa);
    sa.reset();
    REQUIRE(!sa);
}
#endif

#define _TYPED_CREATE_NOTHROW(type)         TestTyped_Create_NoThrow<type>();
#define _TYPED_CREATE_FAILFAST(type)        TestTyped_Create_FailFast<type>();
#define _TYPED_CREATE(type)                 TestTyped_Create<type>();
#define _CREATE_NOTHROW(type)               Test_Create_NoThrow<type>();
#define _CREATE_FAILFAST(type)              Test_Create_FailFast<type>();
#define _CREATE(type)                       Test_Create<type>();

template <typename T>
auto GetReadable(const T& t) -> const T&
{
    return t;
}

template<typename T, typename = typename wistd::enable_if<wistd::is_same<T, wil::unique_bstr>::value, int>::type>
auto GetReadable(const wil::unique_bstr& t) -> BSTR
{
    return t.get();
}

template <typename T>
auto GetWritable(T& t) -> T&
{
    return t;
}

template<typename T, typename = typename wistd::enable_if<wistd::is_same<T, wil::unique_bstr>::value, int>::type>
auto GetWritable(wil::unique_bstr& t) -> BSTR&
{
    return *(t.addressof());
}


template<typename safearray_t>
void TestTyped_DirectElement_NoThrow()
{
    auto sample_data = GetSampleData<safearray_t::elemtype>();
    auto SIZE = ULONG{ sample_data.size() };

    using array_type = decltype(sample_data);
    using data_type = typename array_type::value_type;

    if (SIZE > 1)
    {
        auto sa = safearray_t{};

        REQUIRE_SUCCEEDED(sa.create(SIZE));
        REQUIRE(sa);

        // Loop through and set the values with put_element
        for (ULONG i = 0; i < SIZE; ++i)
        {
            REQUIRE_SUCCEEDED(sa.put_element(i, GetReadable(sample_data[i])));
        }

        // Loop through and get the values with get_element
        for (ULONG i = 0; i < SIZE; ++i)
        {
            {
                auto temp = data_type{};
                // And make sure it was the value that was set
                REQUIRE_SUCCEEDED(sa.get_element(i, GetWritable(temp)));
                REQUIRE(PerformCompare(temp, sample_data[i]));
            }
        }
    }
}

#define _TYPED_DIRECT_NOTHROW(type)         TestTyped_DirectElement_NoThrow<type>();


TEST_CASE("Safearray::Create", "[safearray]")
{
    SECTION("Create SafeArray - No Throw")
    {
        RUN_TYPED_TEST_NOTHROW(_TYPED_CREATE_NOTHROW);
        RUN_TEST(_CREATE_NOTHROW);
    }

    SECTION("Create SafeArray - FailFast")
    {
        RUN_TYPED_TEST_FAILFAST(_TYPED_CREATE_FAILFAST);
        RUN_TEST(_CREATE_FAILFAST);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Create SafeArray - Exceptions")
    {
        RUN_TYPED_TEST(_TYPED_CREATE);
        RUN_TEST(_CREATE);
    }
#endif
}


TEST_CASE("Safearray::Put/Get", "[safearray]")
{
    SECTION("Direct Element Access - No Throw")
    {
        RUN_TYPED_TEST_NOTHROW(_TYPED_DIRECT_NOTHROW);
        //RUN_TEST(_CREATE_NOTHROW);
    }

    SECTION("Direct Element Access - FailFast")
    {
        //RUN_TYPED_TEST_FAILFAST(_TYPED_CREATE_FAILFAST);
        //RUN_TEST(_CREATE_FAILFAST);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Direct Element Access - Exceptions")
    {
        //RUN_TYPED_TEST(_TYPED_CREATE);
        //RUN_TEST(_CREATE);
    }
#endif
}

