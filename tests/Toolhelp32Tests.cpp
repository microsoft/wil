#include "common.h"
#include <WinUser.h>
#include <wil/toolhelp32.h>
#include <cstring>

TEST_CASE("Toolhelp32", "[EnumProcesses]")
{
    wil::for_each_process([](PROCESSENTRY32 entry) {
        REQUIRE_FALSE(std::strlen(entry.szExeFile) == 0);
    });
}

TEST_CASE("Toolhelp32", "[EnumModules]")
{
    wil::for_each_module([](MODULEENTRY32 entry) {
        REQUIRE_FALSE(std::strlen(entry.szExePath) == 0);
    });
}

TEST_CASE("Toolhelp32", "[EnumThreads]")
{
    wil::for_each_thread([](THREADENTRY32 entry) {
        REQUIRE_FALSE(entry.th32ThreadID == 0);
    });
}

TEST_CASE("Toolhelp32", "[EnumHeaps]")
{
    wil::for_each_heap([](HEAPLIST32 entry) {
        REQUIRE_FALSE(entry.th32HeapID == 0);
    });
}