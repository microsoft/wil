#include "common.h"
#include <wil/windowing.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WIL_HAS_CXX_17
TEST_CASE("EnumWindows", "[windowing]")
{
    // lambda can return a bool
    wil::for_each_window([](HWND hwnd)
        {
            REQUIRE(IsWindow(hwnd));
            return true;
        });

    // or not return anything (we'll stop if we get an exception)
    wil::for_each_window([](HWND hwnd)
        {
            REQUIRE(IsWindow(hwnd));
        });

    // With captures
    const auto pid = GetCurrentProcessId();
    wil::for_each_window([pid](HWND hwnd)
        {
            if (pid == GetWindowThreadProcessId(hwnd, nullptr))
            {
                REQUIRE(IsWindow(hwnd));
            };
            return true;
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

    // lambda can also return an HRESULT and we'll stop if it's not S_OK
    wil::for_each_thread_window(thread_id, [](HWND hwnd)
        {
            REQUIRE(IsWindow(hwnd));
            return S_FALSE;
        });
}
#endif