
#include <wil/resource.h>
#include <wil/Safearrays.h>

#include "common.h"

#define TEST_ALL_SA_TYPES(fn)      \
    (fn)<wil::unique_char_safearray_nothrow>();   \
    fn<wil::unique_double_safearray();          \
    fn < wil::unique_safearray();

template<typename safearray_t>
inline void TestCreate()
{
    constexpr auto SIZE = 256;

    auto sa = safearray_t{};
    LONG val = 0;
    ULONG count = 0;
    if constexpr (wistd::is_same<void, safearray_t::elemtype>::value)
    {
        REQUIRE_SUCCEEDED(sa.create(VT_UI1, SIZE));
    }
    else
    {
        REQUIRE_SUCCEEDED(sa.create(SIZE));
    }
    REQUIRE(sa);
    REQUIRE(sa.dim() == 1);
    REQUIRE(sa.elemsize() == 1);
    if constexpr (wistd::is_same<safearray_t::policy, err_exception_policy>::value)
    {
        REQUIRE_NOTHROW(sa.count() == SIZE);
        REQUIRE_NOTHROW(sa.lbound() == 0);
        REQUIRE_NOTHROW(sa.ubound() == SIZE - 1);
    }
    else if constexpr (wistd::is_same<safearray_t::policy, err_failfast_policy>::value)
    {
        REQUIRE_NOCRASH(sa.count(&count));
        REQUIRE(count == SIZE);
        REQUIRE_NOCRASH(sa.lbound(&val));
        REQUIRE(val == 0);
        REQUIRE_NOCRASH(sa.ubound(&val));
        REQUIRE(val == SIZE - 1);
    }
    else
    {
        REQUIRE_SUCCEEDED(sa.count(&count));
        REQUIRE(count == SIZE);
        REQUIRE_SUCCEEDED(sa.lbound(&val));
        REQUIRE(val == 0);
        REQUIRE_SUCCEEDED(sa.ubound(&val));
        REQUIRE(val == SIZE - 1);
    }

    sa.reset();
    REQUIRE(!sa);
}

//template<typename safearray_t>
//void TestCreate()
//{
//    constexpr auto SIZE = 256;
//
//    auto sa = safearray_t{};
//    LONG val = 0;
//    ULONG count = 0;
//    REQUIRE_SUCCEEDED(sa.create(VT_UI1, SIZE));
//    REQUIRE(sa);
//    REQUIRE(sa.dim() == 1);
//    REQUIRE(sa.elemsize() == 1);
//    REQUIRE_SUCCEEDED(sa.count(&count));
//    REQUIRE(count == SIZE);
//    REQUIRE_SUCCEEDED(sa.lbound(&val));
//    REQUIRE(val == 0);
//    REQUIRE_SUCCEEDED(sa.ubound(&val));
//    REQUIRE(val == SIZE - 1);
//
//    sa.reset();
//    REQUIRE(!sa);
//}

TEST_CASE("SafearrayTests::Create", "[safearray][create]")
{
    TEST_ALL_SA_TYPES(TestCreate);
//    SECTION("Create SafeArray - No Throw")
//    {
//    }
//
//#ifdef WIL_ENABLE_EXCEPTIONS
//    SECTION("Create SafeArray - Exceptions")
//    {
//        constexpr auto SIZE = 256;
//
//        wil::unique_safearray sa;
//
//        REQUIRE_NOTHROW(sa.create(VT_UI1, SIZE));
//        REQUIRE(sa.dim() == 1);
//        REQUIRE(sa.elemsize() == 1);
//        REQUIRE(sa.count() == SIZE);
//        REQUIRE(sa.lbound() == 0);
//        REQUIRE(sa.ubound() == SIZE-1);
//
//        sa.reset();
//        REQUIRE(!sa);
//    }
//#endif
//
//    SECTION("Create Typed SafeArray - No Throw")
//    {
//        constexpr auto SIZE = 256;
//        auto fn = [](auto& sa)
//        {
//            REQUIRE_SUCCEEDED(sa.create(SIZE));
//            LONG val = 0;
//            ULONG count = 0;
//            REQUIRE(sa.dim() == 1);
//            REQUIRE(sa.elemsize() == 1);
//            REQUIRE_SUCCEEDED(sa.count(&count));
//            REQUIRE(count == SIZE);
//            REQUIRE_SUCCEEDED(sa.lbound(&val));
//            REQUIRE(val == 0);
//            REQUIRE_SUCCEEDED(sa.ubound(&val));
//            REQUIRE(val == SIZE - 1);
//
//            sa.reset();
//            REQUIRE(!sa);
//        };
//    }
}

template<typename safearray_t>
void SafeArrayLockTests()
{
    auto sa = safearray_t{};
    sa.create(VT_UI1, 16);
    REQUIRE(sa);
    const auto startingLocks = sa.get()->cLocks;
    {
        auto lock = sa.scope_lock();
        REQUIRE(sa.get()->cLocks > startingLocks); // Verify Lock Count increased
    }
    REQUIRE(startingLocks == sa.get()->cLocks);   // Verify it dropped back down
}

TEST_CASE("SafearrayTests::Lock", "[safearray][lock]")
{
    SECTION("Lock SafeArray")
    {
        SafeArrayLockTests<wil::unique_safearray_nothrow>();
        SafeArrayLockTests<wil::unique_safearray_failfast>();
#ifdef WIL_ENABLE_EXCEPTIONS
        SafeArrayLockTests<wil::unique_safearray>();
#endif
    }

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
