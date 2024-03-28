#include "resource.h"
#ifndef __WIL_TOOLHELP32_INCLUDED
#define __WIL_TOOLHELP32_INCLUDED
#include <TlHelp32.h>
#include <processthreadsapi.h>
namespace wil
{
namespace details
{
    template <typename TEntry, typename TEnumApi, typename TCallback>
    void do_enum_snapshot(HANDLE handle, TEntry& entry, TEnumApi&& enumApiFirst, TEnumApi&& enumApiNext, TCallback&& callback)
    {
        using result_t = decltype(callback(TEntry{}));
        bool enumResult = enumApiFirst(handle, &entry);
        if (!enumResult)
            return;

        do
        {
            if constexpr (wistd::is_void_v<result_t>)
            {
                callback(entry);
            }
            else if constexpr (wistd::is_same_v<result_t, bool>)
            {
                if (callback(entry))
                    return;
            }
            else
            {
                static_assert(
                    [] {
                        return false;
                    }(),
                    "Callback must return void or bool");
            }
            enumResult = enumApiNext(handle, &entry);
        } while (enumResult);
    }
} // namespace details

template <typename TCallback>
void for_each_process(TCallback&& callback)
{
    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    details::do_enum_snapshot(
        unique_handle{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)}.get(),
        entry,
        &Process32First,
        &Process32Next,
        wistd::forward<TCallback>(callback));
}

template <typename TCallback>
void for_each_thread(TCallback&& callback)
{
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    details::do_enum_snapshot(
        unique_handle{CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)}.get(),
        entry,
        &Thread32First,
        &Thread32Next,
        wistd::forward<TCallback>(callback));
}

template <typename TCallback>
void for_each_module(TCallback&& callback, bool include32For64Bit = false)
{
    MODULEENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    details::do_enum_snapshot(
        unique_handle{CreateToolhelp32Snapshot(include32For64Bit ? TH32CS_SNAPMODULE32 : TH32CS_SNAPMODULE, 0)}.get(),
        entry,
        &Module32First,
        &Module32Next,
        wistd::forward<TCallback>(callback));
}

template <typename TCallback>
void for_each_heap_list(TCallback&& callback)
{
    HEAPLIST32 entry{};
    entry.dwSize = sizeof(entry);
    details::do_enum_snapshot(
        unique_handle{CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, 0)}.get(),
        entry,
        &Heap32ListFirst,
        &Heap32ListNext,
        wistd::forward<TCallback>(callback));
}

template <typename TCallback>
void for_each_heap(TCallback&& callback, ULONG_PTR heapId, DWORD pid = GetCurrentProcessId())
{
    using result_t = decltype(callback(HEAPENTRY32{}));

    HEAPENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    bool enumResult = Heap32First(&entry, pid, heapId);
    do
    {
        if constexpr (wistd::is_void_v<result_t>)
        {
            callback(entry);
        }
        else if constexpr (wistd::is_same_v<result_t, bool>)
        {
            if (callback(entry))
                return;
        }
        else
        {
            static_assert(
                [] {
                    return false;
                }(),
                "Callback must return void or bool");
        }
        enumResult = Heap32Next(&entry);
    } while (enumResult);
}

template <typename TCallback>
void for_each_heap(TCallback&& callback, HEAPLIST32 const& heapList, DWORD pid = GetCurrentProcessId())
{
    for_each_heap(wistd::forward<TCallback>(callback), heapList.th32HeapID, pid);
}
} // namespace wil

#endif