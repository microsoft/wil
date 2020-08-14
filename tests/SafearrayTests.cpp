
#include <wil/resource.h>
#include <wil/Safearrays.h>

#include "common.h"

#define RUN_TEST_TYPED_NOTHROW(test)        WI_FOREACH(test,                                \
                                                wil::unique_char_safearray_nothrow,         \
                                                wil::unique_short_safearray_nothrow,        \
                                                wil::unique_long_safearray_nothrow,         \
                                                wil::unique_int_safearray_nothrow,          \
                                                wil::unique_longlong_safearray_nothrow,     \
                                                wil::unique_byte_safearray_nothrow,         \
                                                wil::unique_word_safearray_nothrow,         \
                                                wil::unique_dword_safearray_nothrow,        \
                                                wil::unique_ulonglong_safearray_nothrow,    \
                                                wil::unique_float_safearray_nothrow,        \
                                                wil::unique_double_safearray_nothrow,       \
                                                wil::unique_bstr_safearray_nothrow,         \
                                                wil::unique_unknown_safearray_nothrow,      \
                                                wil::unique_dispatch_safearray_nothrow,     \
                                                wil::unique_variant_safearray_nothrow)

#define RUN_TEST_TYPED_FAILFAST(test)        WI_FOREACH(test,                               \
                                                wil::unique_char_safearray_failfast,        \
                                                wil::unique_short_safearray_failfast,       \
                                                wil::unique_long_safearray_failfast,        \
                                                wil::unique_int_safearray_failfast,         \
                                                wil::unique_longlong_safearray_failfast,    \
                                                wil::unique_byte_safearray_failfast,        \
                                                wil::unique_word_safearray_failfast,        \
                                                wil::unique_dword_safearray_failfast,       \
                                                wil::unique_ulonglong_safearray_failfast,   \
                                                wil::unique_float_safearray_failfast,       \
                                                wil::unique_double_safearray_failfast,      \
                                                wil::unique_bstr_safearray_failfast,        \
                                                wil::unique_unknown_safearray_failfast,     \
                                                wil::unique_dispatch_safearray_failfast,    \
                                                wil::unique_variant_safearray_failfast)

#define RUN_TEST_TYPED(test)                WI_FOREACH(test,                                \
                                                wil::unique_char_safearray,                 \
                                                wil::unique_short_safearray,                \
                                                wil::unique_long_safearray,                 \
                                                wil::unique_int_safearray,                  \
                                                wil::unique_longlong_safearray,             \
                                                wil::unique_byte_safearray,                 \
                                                wil::unique_word_safearray,                 \
                                                wil::unique_dword_safearray,                \
                                                wil::unique_ulonglong_safearray,            \
                                                wil::unique_float_safearray,                \
                                                wil::unique_double_safearray,               \
                                                wil::unique_bstr_safearray,                 \
                                                wil::unique_unknown_safearray,              \
                                                wil::unique_dispatch_safearray,             \
                                                wil::unique_variant_safearray)

template<typename safearray_t>
void TestCreateTyped_NoThrow()
{
    constexpr auto SIZE = 256;

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
    {
        const auto startingLocks = sa.get()->cLocks;
        {
            auto lock = sa.scope_lock();
            REQUIRE(sa.get()->cLocks > startingLocks); // Verify Lock Count increased
        }
        REQUIRE(startingLocks == sa.get()->cLocks);   // Verify it dropped back down
    }
    sa.reset();
    REQUIRE(!sa);
}

template<typename safearray_t>
void TestCreateTyped_FailFast()
{
    constexpr auto SIZE = 256;

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
    {
        const auto startingLocks = sa.get()->cLocks;
        {
            auto lock = sa.scope_lock();
            REQUIRE(sa.get()->cLocks > startingLocks); // Verify Lock Count increased
        }
        REQUIRE(startingLocks == sa.get()->cLocks);   // Verify it dropped back down
    }
    sa.reset();
    REQUIRE(!sa);
}

#ifdef WIL_ENABLE_EXCEPTIONS
template<typename safearray_t>
void TestCreateTyped()
{
    constexpr auto SIZE = 256U;

    safearray_t sa;
    REQUIRE_NOTHROW(sa = safearray_t{ SIZE });
    REQUIRE(sa);
    REQUIRE(sa.dim() == 1);
    REQUIRE(sa.elemsize() == sizeof(safearray_t::elemtype));
    REQUIRE_NOTHROW(sa.count() == SIZE);
    REQUIRE_NOTHROW(sa.lbound() == 0);
    REQUIRE_NOTHROW(sa.ubound() == SIZE-1);
    {
        const auto startingLocks = sa.get()->cLocks;
        {
            auto lock = sa.scope_lock();
            REQUIRE(sa.get()->cLocks > startingLocks); // Verify Lock Count increased
        }
        REQUIRE(startingLocks == sa.get()->cLocks);   // Verify it dropped back down
    }
    sa.reset();
    REQUIRE(!sa);
}
#endif

#define _CREATE_TYPED_NOTHROW(type)         TestCreateTyped_NoThrow<type>();
#define _CREATE_TYPED_FAILFAST(type)        TestCreateTyped_FailFast<type>();
#define _CREATE_TYPED(type)                 TestCreateTyped<type>();


TEST_CASE("SafearrayTests::Create", "[safearray][create]")
{
    SECTION("Create SafeArray - No Throw")
    {
        RUN_TEST_TYPED_NOTHROW(_CREATE_TYPED_NOTHROW);
    }

    SECTION("Create SafeArray - FailFast")
    {
        RUN_TEST_TYPED_FAILFAST(_CREATE_TYPED_FAILFAST);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Create SafeArray - Exceptions")
    {
        RUN_TEST_TYPED(_CREATE_TYPED);
    }
#endif
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
