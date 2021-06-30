#pragma once
//
//! @file
//! WIL Error Handling Helpers: supporting file defining a family of macros and functions designed
//! to uniformly handle errors across return codes, fail fast, exceptions and logging

#ifndef __MICROSOFT_WINDOWS_TELEMETRY_RESULTMACROS_INCLUDED
#define __MICROSOFT_WINDOWS_TELEMETRY_RESULTMACROS_INCLUDED

#include <wil/result_macros.h>

/// @cond
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
#if !defined(WIL_SUPPRESS_PRIVATE_API_USE) && !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE)
__declspec(selectany) HMODULE g_wil_details_ntdllModuleHandle = NULL;
__declspec(selectany) HMODULE g_wil_details_kernelbaseModuleHandle = NULL;

// ordinarily caching a module handle like this would be unsafe (could be unloaded) but ntdll
// and kernelbase cannot be unloaded from a process.
inline HMODULE wil_details_GetNtDllModuleHandle() WI_NOEXCEPT
{
    if (!g_wil_details_ntdllModuleHandle)
    {
        g_wil_details_ntdllModuleHandle = GetModuleHandleW(L"ntdll.dll");
    }
    return g_wil_details_ntdllModuleHandle;
}
inline HMODULE wil_details_GetKernelBaseModuleHandle() WI_NOEXCEPT
{
    if (!g_wil_details_kernelbaseModuleHandle)
    {
        g_wil_details_kernelbaseModuleHandle = GetModuleHandleW(L"kernelbase.dll");
    }
    return g_wil_details_kernelbaseModuleHandle;
}
#endif
#endif
/// @endcond

#if defined(__cplusplus) && !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE)

#pragma warning(push)
#pragma warning(disable:4714 6262)    // __forceinline not honored, stack size

#ifndef WIL_SUPPRESS_PRIVATE_API_USE

#if !defined(_NTSYSTEM_)
#define NTSYSAPI     DECLSPEC_IMPORT
#else
#define NTSYSAPI
#endif

extern "C" NTSYSAPI VOID NTAPI LdrFastFailInLoaderCallout(VOID);
#endif

namespace wil
{
    /// @cond
    namespace details
    {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
#ifndef WIL_SUPPRESS_PRIVATE_API_USE
        inline void __stdcall FailFastInLoaderCallout() WI_NOEXCEPT
        {
            // GetProcAddress is used here since we may be linking against an ntdll that doesn't contain this function
            // e.g. if this header is used on an OS older than Threshold (which may be the case for IE)
            auto pfn = reinterpret_cast<decltype(&::LdrFastFailInLoaderCallout)>(GetProcAddress(wil_details_GetNtDllModuleHandle(), "LdrFastFailInLoaderCallout"));
            if (pfn != nullptr)
            {
                pfn();  // don't do anything non-trivial from DllMain, fail fast.
            }
        }

        inline ULONG __stdcall RtlNtStatusToDosErrorNoTeb(_In_ NTSTATUS status) WI_NOEXCEPT
        {
            static decltype(RtlNtStatusToDosErrorNoTeb)* s_pfnRtlNtStatusToDosErrorNoTeb = nullptr;
            if (s_pfnRtlNtStatusToDosErrorNoTeb == nullptr)
            {
                s_pfnRtlNtStatusToDosErrorNoTeb = reinterpret_cast<decltype(RtlNtStatusToDosErrorNoTeb)*>(GetProcAddress(wil_details_GetNtDllModuleHandle(), "RtlNtStatusToDosErrorNoTeb"));
            }
            return s_pfnRtlNtStatusToDosErrorNoTeb ? s_pfnRtlNtStatusToDosErrorNoTeb(status) : 0;
        }

        inline BOOLEAN __stdcall RtlDllShutdownInProgress() WI_NOEXCEPT
        {
            static decltype(RtlDllShutdownInProgress)* s_pfnRtlDllShutdownInProgress = nullptr;
            if (s_pfnRtlDllShutdownInProgress == nullptr)
            {
                s_pfnRtlDllShutdownInProgress = reinterpret_cast<decltype(RtlDllShutdownInProgress)*>(GetProcAddress(wil_details_GetNtDllModuleHandle(), "RtlDllShutdownInProgress"));
            }
            return s_pfnRtlDllShutdownInProgress ? s_pfnRtlDllShutdownInProgress() : FALSE;
        }

        inline NTSTATUS __stdcall RtlDisownModuleHeapAllocation(_In_ HANDLE heapHandle, _In_ PVOID address) WI_NOEXCEPT
        {
            static decltype(RtlDisownModuleHeapAllocation)* s_pfnRtlDisownModuleHeapAllocation = nullptr;
            if (s_pfnRtlDisownModuleHeapAllocation == nullptr)
            {
                s_pfnRtlDisownModuleHeapAllocation = reinterpret_cast<decltype(RtlDisownModuleHeapAllocation)*>(GetProcAddress(wil_details_GetNtDllModuleHandle(), "RtlDisownModuleHeapAllocation"));
            }
            return s_pfnRtlDisownModuleHeapAllocation ? s_pfnRtlDisownModuleHeapAllocation(heapHandle, address) : STATUS_SUCCESS;
        }

        inline void __stdcall FormatNtStatusMsg(NTSTATUS status,
            _Out_writes_(messageStringSizeChars) _Post_z_ PWSTR messageString,
            _Pre_satisfies_(messageStringSizeChars > 0) DWORD messageStringSizeChars)
        {
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                wil_details_GetNtDllModuleHandle(), status, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                messageString, messageStringSizeChars, nullptr);
        }


#endif // !DEFINED(WIL_SUPPRESS_PRIVATE_API_USE)
#endif  // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
    } // details namespace
    /// @endcond

    //*****************************************************************************
    // Calling WilInitialize_ResultMacros_DesktopOrSystem additionally provides:
    // - FAIL_FAST_IMMEDIATE_IF_IN_LOADER_CALLOUT enforcement
    // - Higher fidelity mapping of NTSTATUS->HRESULT for RETURN_IF_NTSTATUS*
    // - wil::ProcessShutdownInProgress returns 'true' during process shutdown (false when not called or set)
    //*****************************************************************************

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
#ifndef WIL_SUPPRESS_PRIVATE_API_USE
    //! Call this method to initialize WIL manually in a module where RESULT_SUPPRESS_STATIC_INITIALIZERS is required. WIL will
    //! use internal methods to provide additional diagnostic information & behavior.
    inline void WilInitialize_ResultMacros_DesktopOrSystem()
    {
        WilInitialize_ResultMacros_DesktopOrSystem_SuppressPrivateApiUse();
        details::g_pfnFailFastInLoaderCallout = details::FailFastInLoaderCallout;
        details::g_pfnRtlNtStatusToDosErrorNoTeb = details::RtlNtStatusToDosErrorNoTeb;
        details::g_pfnDllShutdownInProgress = details::RtlDllShutdownInProgress;
        details::g_pfnRtlDisownModuleHeapAllocation = details::RtlDisownModuleHeapAllocation;
        details::g_pfnFormatNtStatusMsg = details::FormatNtStatusMsg;
    }
#endif // WIL_SUPPRESS_PRIVATE_API_USE

    /// @cond
    namespace details
    {
#ifndef RESULT_SUPPRESS_STATIC_INITIALIZERS
#ifndef WIL_SUPPRESS_PRIVATE_API_USE
        WI_HEADER_INITITALIZATION_FUNCTION(WilInitialize_ResultMacros_DesktopOrSystem, []
            {
                ::wil::WilInitialize_ResultMacros_DesktopOrSystem();
                return 1;
            });
#endif // WIL_SUPPRESS_PRIVATE_API_USE
#endif // RESULT_SUPPRESS_STATIC_INITIALIZERS
    }
    /// @endcond
#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

} // namespace wil

#pragma warning(pop)

#endif // defined(__cplusplus) && !defined(__WIL_MIN_KERNEL) && !defined(WIL_KERNEL_MODE)

#endif // __MICROSOFT_WINDOWS_TELEMETRY_RESULTMACROS_INCLUDED