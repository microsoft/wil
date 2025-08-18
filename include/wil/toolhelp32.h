//*********************************************************
//
//    Copyright (c) Microsoft. All rights reserved.
//    This code is licensed under the MIT License.
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
//    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//    PARTICULAR PURPOSE AND NONINFRINGEMENT.
//
//*********************************************************
#ifndef __WIL_TOOLHELP32_INCLUDED
#define __WIL_TOOLHELP32_INCLUDED
#include <TlHelp32.h>
#include <processthreadsapi.h>
#include "resource.h"
namespace wil
{
namespace details
{
    template <typename TEntry, typename TEnumApi, typename TCallback>
    auto do_enum_snapshot(HANDLE handle, TEntry& entry, TEnumApi&& enumApiFirst, TEnumApi&& enumApiNext, TCallback&& callback)
    {
        if (handle == INVALID_HANDLE_VALUE)
            return E_HANDLE;

        using result_t = decltype(callback(TEntry{}));
        bool enumResult = enumApiFirst(handle, &entry);
        if (!enumResult)
            return E_ABORT;

        do
        {
            if constexpr (wistd::is_void_v<result_t>)
            {
                callback(entry);
            }
            else if constexpr (wistd::is_same_v<result_t, bool>)
            {
                if (callback(entry))
                    return S_OK;
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
        return S_OK;
    }
} // namespace details

#pragma region Process
template <typename TCallback>
auto for_each_process_nothrow(TCallback&& callback)
{
    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    return details::do_enum_snapshot(
        unique_handle{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)}.get(),
        entry,
        &Process32First,
        &Process32Next,
        wistd::forward<TCallback>(callback));
}

template <typename TCallback>
void for_each_process_failfast(TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_process_nothrow(callback));
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename TCallback>
void for_each_process(TCallback&& callback)
{
    THROW_IF_FAILED(for_each_process_nothrow(callback));
}
#endif
#pragma endregion

#pragma region Thread
template <typename TCallback>
auto for_each_system_thread_nothrow(TCallback&& callback)
{
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    return details::do_enum_snapshot(
        unique_handle{CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)}.get(),
        entry,
        &Thread32First,
        &Thread32Next,
        wistd::forward<TCallback>(callback));
}

template <typename TCallback>
void for_each_system_thread_failfast(TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_system_thread_nothrow(callback));
}

template <typename TCallback>
auto for_each_process_thread_nothrow(DWORD pid, TCallback&& callback)
{
    return for_each_system_thread_nothrow([&](THREADENTRY32 const& entry) {
        if (entry.th32OwnerProcessID == pid)
            callback(entry);
    });
}

template <typename TCallback>
auto for_each_process_thread_nothrow(TCallback&& callback)
{
    return for_each_process_thread_nothrow(GetCurrentProcessId(), callback);
}

template <typename TCallback>
void for_each_process_thread_failfast(DWORD pid, TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_process_thread_nothrow(pid, callback));
}

template <typename TCallback>
void for_each_process_thread_failfast(TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_process_thread_nothrow(callback));
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename TCallback>
void for_each_system_thread(TCallback&& callback)
{
    THROW_IF_FAILED(for_each_system_thread_nothrow(callback));
}

template <typename TCallback>
void for_each_process_thread(DWORD pid, TCallback&& callback)
{
    THROW_IF_FAILED(for_each_process_thread_nothrow(pid, callback));
}

template <typename TCallback>
void for_each_process_thread(TCallback&& callback)
{
    THROW_IF_FAILED(for_each_process_thread_nothrow(callback));
}

#endif
#pragma endregion

#pragma region Module
template <typename TCallback>
auto for_each_module_nothrow(DWORD pid, bool include32BitModule, TCallback&& callback)
{
    MODULEENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    return details::do_enum_snapshot(
        unique_handle{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | (include32BitModule ? TH32CS_SNAPMODULE32 : 0), pid)}.get(),
        entry,
        &Module32First,
        &Module32Next,
        wistd::forward<TCallback>(callback));
}

template <typename TCallback>
auto for_each_module_nothrow(bool include32BitModule, TCallback&& callback)
{
    return for_each_module_nothrow(0, include32BitModule, callback);
}

template <typename TCallback>
auto for_each_module_nothrow(TCallback&& callback)
{
    return for_each_module_nothrow(true, callback);
}

template <typename TCallback>
void for_each_module_failfast(DWORD pid, bool include32BitModule, TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_module_nothrow(pid, include32BitModule, callback));
}

template <typename TCallback>
void for_each_module_failfast(bool include32BitModule, TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_module_nothrow(0, include32BitModule, callback));
}

template <typename TCallback>
void for_each_module_failfast(TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_module_nothrow(callback));
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename TCallback>
void for_each_module(DWORD pid, bool include32BitModule, TCallback&& callback)
{
    THROW_IF_FAILED(for_each_module_nothrow(pid, include32BitModule, callback));
}

template <typename TCallback>
void for_each_module(bool include32BitModule, TCallback&& callback)
{
    THROW_IF_FAILED(for_each_module_nothrow(0, include32BitModule, callback));
}

template <typename TCallback>
void for_each_module(TCallback&& callback)
{
    THROW_IF_FAILED(for_each_module_nothrow(callback));
}
#endif
#pragma endregion

#pragma region HeapList
template <typename TCallback>
auto for_each_heap_list_nothrow(DWORD pid, TCallback&& callback)
{
    HEAPLIST32 entry{};
    entry.dwSize = sizeof(entry);
    return details::do_enum_snapshot(
        unique_handle{CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, pid)}.get(),
        entry,
        &Heap32ListFirst,
        &Heap32ListNext,
        wistd::forward<TCallback>(callback));
}

template <typename TCallback>
auto for_each_heap_list_nothrow(TCallback&& callback)
{
    return for_each_heap_list_nothrow(0, callback);
}

template <typename TCallback>
void for_each_heap_list_failfast(DWORD pid, TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_heap_list_nothrow(pid, callback));
}

template <typename TCallback>
void for_each_heap_list_failfast(TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_heap_list_nothrow(callback));
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename TCallback>
void for_each_heap_list(DWORD pid, TCallback&& callback)
{
    THROW_IF_FAILED(for_each_heap_list_nothrow(pid, callback));
}

template <typename TCallback>
void for_each_heap_list(TCallback&& callback)
{
    THROW_IF_FAILED(for_each_heap_list_nothrow(callback));
}
#endif
#pragma endregion

#pragma region Heap
template <typename TCallback>
auto for_each_heap_nothrow(DWORD pid, ULONG_PTR heapId, TCallback&& callback)
{
    using result_t = decltype(callback(HEAPENTRY32{}));

    HEAPENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    if (!Heap32First(&entry, pid, heapId))
        return E_ABORT;

    bool enumResult = true;
    do
    {
        if constexpr (wistd::is_void_v<result_t>)
        {
            callback(entry);
        }
        else if constexpr (wistd::is_same_v<result_t, bool>)
        {
            if (callback(entry))
                return S_OK;
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
    return S_OK;
}

template <typename TCallback>
auto for_each_heap_nothrow(ULONG_PTR heapId, TCallback&& callback)
{
    return for_each_heap_nothrow(GetCurrentProcessId(), heapId, callback);
}

template <typename TCallback>
void for_each_heap_failfast(DWORD pid, ULONG_PTR heapId, TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_heap_nothrow(pid, heapId, callback));
}

template <typename TCallback>
void for_each_heap_failfast(ULONG_PTR heapId, TCallback&& callback)
{
    FAIL_FAST_IF_FAILED(for_each_heap_nothrow(heapId, callback));
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename TCallback>
void for_each_heap(DWORD pid, ULONG_PTR heapId, TCallback&& callback)
{
    THROW_IF_FAILED(for_each_heap_nothrow(pid, heapId, callback));
}

template <typename TCallback>
void for_each_heap(ULONG_PTR heapId, TCallback&& callback)
{
    THROW_IF_FAILED(for_each_heap_nothrow(heapId, callback));
}
#endif
#pragma endregion
} // namespace wil

#endif