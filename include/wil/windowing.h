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

#ifndef WIL_ENABLE_EXCEPTIONS
#error This header requires exceptions
#endif

#include <WinUser.h>
#include <exception>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
namespace wil
{
namespace details
{
    template <typename T>
    struct always_false : wistd::false_type
    {
    };

    template <typename TEnumApi, typename TCallback>
    void CallCallbackNoThrow(TEnumApi&& enumApi, TCallback&& callback) noexcept
    {
        auto enumproc = [](HWND hwnd, LPARAM lParam) -> BOOL {
            auto pCallback = reinterpret_cast<TCallback*>(lParam);
            using result_t = decltype((*pCallback)(hwnd));
#ifdef __cpp_if_constexpr
            if constexpr (std::is_void_v<result_t>)
            {
                (*pCallback)(hwnd);
                return TRUE;
            }
            else if constexpr (std::is_same_v<result_t, HRESULT>)
            {
                // NB: this works for both HRESULT and NTSTATUS as both S_OK and ERROR_SUCCESS are 0
                return (S_OK == (*pCallback)(hwnd)) ? TRUE : FALSE;
            }
            else if constexpr (std::is_same_v<result_t, bool>)
            {
                return (*pCallback)(hwnd) ? TRUE : FALSE;
            }
            else
            {
                static_assert(details::always_false<TCallback>::value, "Callback must return void, bool, or HRESULT");
            }
#else
            return (*pCallback)(hwnd);
#endif
        };
        enumApi(enumproc, reinterpret_cast<LPARAM>(&callback));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    template <typename TEnumApi, typename TCallback>
    void CallCallback(TEnumApi&& enumApi, TCallback&& callback)
    {
        struct
        {
            std::exception_ptr exception;
            TCallback* pCallback;
        } callbackData = {nullptr, &callback};
        auto enumproc = [](HWND hwnd, LPARAM lParam) -> BOOL {
            auto pCallbackData = reinterpret_cast<decltype(&callbackData)>(lParam);
            try
            {
                auto pCallback = pCallbackData->pCallback;
                using result_t = decltype((*pCallback)(hwnd));
#ifdef __cpp_if_constexpr
                if constexpr (std::is_void_v<result_t>)
                {
                    (*pCallback)(hwnd);
                    return TRUE;
                }
                else if constexpr (std::is_same_v<result_t, HRESULT>)
                {
                    // NB: this works for both HRESULT and NTSTATUS as both S_OK and ERROR_SUCCESS are 0
                    return (S_OK == (*pCallback)(hwnd)) ? TRUE : FALSE;
                }
                else if constexpr (std::is_same_v<result_t, bool>)
                {
                    return (*pCallback)(hwnd) ? TRUE : FALSE;
                }
                else
                {
                    static_assert(details::always_false<TCallback>::value, "Callback must return void, bool, or HRESULT");
                }
#else
                return (*pCallback)(hwnd);
#endif
            }
            catch (...)
            {
                pCallbackData->exception = std::current_exception();
                return FALSE;
            }
        };
        enumApi(enumproc, reinterpret_cast<LPARAM>(&callbackData));
        if (callbackData.exception)
        {
            std::rethrow_exception(callbackData.exception);
        }
    }
#endif
} // namespace details

template <typename TCallback>
auto for_each_window_nothrow(TCallback&& callback) noexcept
{
    details::CallCallbackNoThrow(&EnumWindows, wistd::forward<TCallback>(callback));
}

template <typename TCallback>
auto for_each_thread_window_nothrow(_In_ DWORD threadId, TCallback&& callback) noexcept
{
    auto boundEnumThreadWindows = [threadId](WNDENUMPROC enumproc, LPARAM lParam) noexcept -> BOOL {
        return EnumThreadWindows(threadId, enumproc, lParam);
    };
    details::CallCallbackNoThrow(boundEnumThreadWindows, wistd::forward<TCallback>(callback));
}

template <typename TCallback>
auto for_each_child_window_nothrow(_In_ HWND hwndParent, TCallback&& callback) noexcept
{
    auto boundEnumChildWindows = [hwndParent](WNDENUMPROC enumproc, LPARAM lParam) noexcept -> BOOL {
        return EnumChildWindows(hwndParent, enumproc, lParam);
    };
    details::CallCallbackNoThrow(boundEnumChildWindows, wistd::forward<TCallback>(callback));
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename TCallback>
auto for_each_window(TCallback&& callback)
{
    details::CallCallback(&EnumWindows, wistd::forward<TCallback>(callback));
}

template <typename TCallback>
auto for_each_thread_window(_In_ DWORD threadId, TCallback&& callback)
{
    auto boundEnumThreadWindows = [threadId](WNDENUMPROC enumproc, LPARAM lParam) noexcept -> BOOL {
        return EnumThreadWindows(threadId, enumproc, lParam);
    };
    details::CallCallback(boundEnumThreadWindows, wistd::forward<TCallback>(callback));
}

template <typename TCallback>
auto for_each_child_window(_In_ HWND hwndParent, TCallback&& callback)
{
    auto boundEnumChildWindows = [hwndParent](WNDENUMPROC enumproc, LPARAM lParam) noexcept -> BOOL {
        return EnumChildWindows(hwndParent, enumproc, lParam);
    };
    details::CallCallback(boundEnumChildWindows, wistd::forward<TCallback>(callback));
}
#endif

} // namespace wil
#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#endif // __WIL_WINDOWING_INCLUDED