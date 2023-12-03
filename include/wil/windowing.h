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
#ifndef __WIL_WINDOWING_INCLUDED
#define __WIL_WINDOWING_INCLUDED

#include <WinUser.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
namespace wil
{
    namespace details
    {
        template<typename TCallback>
        BOOL CALLBACK LambdaEnumWindowsProc(HWND data, LPARAM lParam) noexcept
        {
            const TCallback* pCallback = reinterpret_cast<const TCallback*>(lParam);
            using result_t = decltype((*pCallback)(data));
#ifdef __cpp_if_constexpr
            if constexpr (std::is_void_v<result_t>)
            {
                try
                {
                    (*pCallback)(data);
                    return TRUE;
                }
                catch (...)
                {
                    return FALSE;
                }
            }
            else if constexpr (std::is_same_v<result_t, HRESULT>)
            {
                return (S_OK == (*pCallback)(data)) ? TRUE : FALSE;
            }
            else
#endif
            {
                return (*pCallback)(data);
            }
        }

        template<typename TEnumWindows, typename TCallback>
        auto for_each_window(const TEnumWindows& enumWindows, const TCallback& callback) noexcept
        {
            return enumWindows(LambdaEnumWindowsProc<TCallback>, reinterpret_cast<LPARAM>(&callback));
        }

        using EnumThreadWindowsType = decltype(&EnumThreadWindows);

        template<typename TCallback>
        auto for_each_thread_window(_In_ EnumThreadWindowsType enumThreadWindows, _In_ DWORD threadId, const TCallback& callback) noexcept
        {
            return enumThreadWindows(threadId, details::LambdaEnumWindowsProc<TCallback>, reinterpret_cast<LPARAM>(&callback));
        }
    }

    template<typename TCallback>
    auto for_each_window(TCallback&& callback) noexcept
    {
        return details::for_each_window(&EnumWindows, wistd::forward<TCallback>(callback));
    }

    template<typename TCallback>
    auto for_each_thread_window(_In_ DWORD threadId, TCallback&& callback) noexcept
    {
        return details::for_each_thread_window(&EnumThreadWindows, threadId, wistd::forward<TCallback>(callback));
    }
}
#endif
#endif