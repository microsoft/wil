#include "common.h"
#include <wil/windowing.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WIL_HAS_CXX_17
TEST_CASE("EnumWindows", "[windowing]")
{
    wil::for_each_window([](HWND hwnd)
        {
            REQUIRE(IsWindow(hwnd));
            return true;
        });

    wil::for_each_window([](HWND hwnd)
        {
            REQUIRE(IsWindow(hwnd));
        });

    const auto thread_id = GetCurrentThreadId();
    wil::for_each_thread_window(thread_id, [](HWND hwnd)
        {
            REQUIRE(IsWindow(hwnd));
            return true;
        });

    wil::for_each_thread_window(thread_id, [](HWND hwnd)
        {
            REQUIRE(IsWindow(hwnd));
        });

    wil::for_each_thread_window(thread_id, [](HWND hwnd)
        {
            REQUIRE(IsWindow(hwnd));
            return S_FALSE;
        });
}
#endif