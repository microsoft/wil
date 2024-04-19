#include "common.h"
#include <WinUser.h>
#include <wil/toolhelp32.h>
#include <cstring>

TEST_CASE("Toolhelp32", "[EnumProcesses]")
{
    wil::for_each_process([](PROCESSENTRY32 const& entry) {
        REQUIRE_FALSE(std::strlen(entry.szExeFile) == 0);
    });
}

TEST_CASE("Toolhelp32", "[EnumModules]")
{
    wil::for_each_module([](MODULEENTRY32 const& entry) {
        REQUIRE_FALSE(std::strlen(entry.szExePath) == 0);
    });
}

TEST_CASE("Toolhelp32", "[EnumThreads]")
{
    wil::for_each_system_thread([pid = GetCurrentProcessId()](THREADENTRY32 const& entry) {
        if (entry.th32OwnerProcessID == pid)
        {
            REQUIRE_FALSE(entry.th32ThreadID == 0);
        }
    });
}

TEST_CASE("Toolhelp32", "[EnumHeapLists]")
{
    wil::for_each_heap_list([](HEAPLIST32 const& entry) {
        REQUIRE_FALSE(entry.th32HeapID == 0);
    });
}

TEST_CASE("Toolhelp32", "[EnumHeap]")
{
    wil::for_each_heap_list([](HEAPLIST32 const& heapListEntry) {
        REQUIRE_FALSE(heapListEntry.th32HeapID == 0);
        wil::for_each_heap(heapListEntry.th32HeapID, [](HEAPENTRY32 const& heapEntry) {
            REQUIRE_FALSE(heapEntry.dwAddress == 0);
        });
        return false;
    });
}