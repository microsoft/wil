
#include <wil/resource.h>
#include <wil/Safearrays.h>

#include "common.h"


TEST_CASE("SafearrayTests::Create", "[safearray][create]")
{
    SECTION("Create SafeArray - No Throw")
    {
        constexpr auto SIZE = 256;

        wil::unique_safearray_nothrow sa;
        LONG val = 0;
        ULONG count = 0;
        REQUIRE_SUCCEEDED(sa.create(VT_UI1, SIZE));
        REQUIRE(sa.dim() == 1);
        REQUIRE(sa.elemsize() == 1);
        REQUIRE_SUCCEEDED(sa.count(&count));
        REQUIRE(count == SIZE);
        REQUIRE_SUCCEEDED(sa.lbound(&val));
        REQUIRE(val == 0);
        REQUIRE_SUCCEEDED(sa.ubound(&val));
        REQUIRE(val == SIZE-1);

        sa.reset();
        REQUIRE(!sa);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Create SafeArray - Exceptions")
    {
        constexpr auto SIZE = 256;

        wil::unique_safearray sa;

        REQUIRE_NOTHROW(sa.create(VT_UI1, SIZE));
        REQUIRE(sa.dim() == 1);
        REQUIRE(sa.elemsize() == 1);
        REQUIRE(sa.count() == SIZE);
        REQUIRE(sa.lbound() == 0);
        REQUIRE(sa.ubound() == SIZE-1);

        sa.reset();
        REQUIRE(!sa);
    }
#endif
}


TEST_CASE("SafearrayTests::Lock", "[safearray][lock]")
{
    SECTION("Lock SafeArray")
    {
        constexpr auto SIZE = 256;

        wil::unique_safearray_nothrow sa;

        REQUIRE_SUCCEEDED(sa.create(VT_UI1, SIZE));

        const auto startingLocks = sa.get()->cLocks;

        // Control lifetime of Lock
        {
            auto lock = sa.scope_lock();

            // Verify Lock Count increased
            REQUIRE(sa.get()->cLocks > startingLocks);
        }

        const auto endingLocks = sa.get()->cLocks;

        REQUIRE(startingLocks == endingLocks);
    }

}
