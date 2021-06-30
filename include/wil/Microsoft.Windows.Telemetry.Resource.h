#pragma once
//
//! @file
//! WIL Resource Wrappers (RAII): Provides a family of smart pointer patterns and resource wrappers to enable customers to
//! consistently use RAII in all code.

// Common.h contains logic and configuration macros common to all consumers in Windows and therefore needs to be
// included before any 'opensource' headers
#include "Microsoft.Windows.Telemetry.Common.h"

#include <wil/resource.h>

// Internal WIL header files that were historically included before WIL was split into public and internal versions.
// These exist only to maintain back-compat
#include "Microsoft.Windows.Telemetry.ResultMacros.h"

#pragma warning(push)
#pragma warning(disable:26135 26110)    // Missing locking annotation, Caller failing to hold lock
#pragma warning(disable:4714)           // __forceinline not honored

#ifndef __MICROSOFT_WINDOWS_TELEMETRY_RESOURCE
#define __MICROSOFT_WINDOWS_TELEMETRY_RESOURCE
namespace wil
{
    /// @cond
#ifndef WIL_HIDE_DEPRECATED_1611
    WIL_WARN_DEPRECATED_1611_PRAGMA(DetachToOptParam)

        // DEPRECATED: Use wil::detach_to_opt_param
        template <typename T, typename TSmartPointer>
    inline void DetachToOptParam(_Out_opt_ T* outParam, TSmartPointer&& smartPtr)
    {
        detach_to_opt_param(outParam, wistd::forward<TSmartPointer>(smartPtr));
    }
#endif
    /// @endcond
}
#endif // __MICROSOFT_WINDOWS_TELEMETRY_RESOURCE

namespace wil
{
#ifdef __WIL_WINBASE_

#ifndef __WIL_INTERNAL_WINBASE_
#define __WIL_INTERNAL_WINBASE_
#ifndef WIL_HIDE_DEPRECATED_1611
    WIL_WARN_DEPRECATED_1611_PRAGMA(unique_hheap_ptr)

        // deprecated, use unique_process_heap_ptr instead as that has the correct name.
        template <typename T>
    using unique_hheap_ptr = wistd::unique_ptr<T, process_heap_deleter>;
#endif
#endif // __WIL_INTERNAL_WINBASE_
#endif // __WIL_WINBASE_

    // do not add new uses of UNIQUE_STRING_VALUE_EXPERIEMENT, see http://osgvsowi/11252630 for details
#if defined(_WINBASE_) && defined(UNIQUE_STRING_VALUE_EXPERIMENT) && !defined(__WIL_UNIQUE_STRING_VALUE_)
#define __WIL_UNIQUE_STRING_VALUE_
    /** Helper to make use of wil::unique_cotaskmem_string, wil::unique_hlocal_string, wil::unique_process_heap_string
    easier to use. Defaults to using unique_cotaskmem_string.

    wil::unique_string_value<> m_value;

    wil::unique_string_value<wil::unique_hlocal_string> m_localAllocValue;
    */
    template<typename string_type = unique_cotaskmem_string>
    class unique_string_value : public string_type
    {
    public:
        typedef string_type string_type;

        unique_string_value() = default;
        unique_string_value(PWSTR value) : string_type(value)
        {
        }

        unique_string_value(string_type&& other) : string_type(wistd::move(other))
        {
        }

        PCWSTR get_not_null() const
        {
            return get() ? get() : L"";
        }

        HRESULT set_nothrow(_In_opt_ PCWSTR value) WI_NOEXCEPT
        {
            if (value)
            {
                reset(wil::make_unique_string_nothrow<string_type>(value).release());
                return get() ? S_OK : E_OUTOFMEMORY; // log errors at the caller of this method.
            }
            else
            {
                reset();
                return S_OK;
            }
        }

        HRESULT copy_to_nothrow(_Outptr_opt_result_maybenull_ PWSTR* result) WI_NOEXCEPT
        {
            unique_string_value<string_type> temp;
            RETURN_IF_FAILED(temp.set_nothrow(get()));
            *result = temp.release();
            return S_OK;
        }

#ifdef WIL_ENABLE_EXCEPTIONS
        void set(_In_opt_ PCWSTR value)
        {
            THROW_IF_FAILED(set_nothrow(value));
        }
#endif

        unique_string_value<string_type>& operator=(string_type&& other)
        {
            string_type::operator=(wistd::move(other));
            return *this;
        }

        /** returns a pointer to and the size of the buffer. Useful for updating
        the value when the updated value is guaranteed to fit based on the size. */
        PWSTR get_dangerous_writeable_buffer(_Out_ size_t* size)
        {
            if (get())
            {
                *size = wcslen(get()) + 1; // include null
                return const_cast<PWSTR>(get());
            }
            else
            {
                *size = 0;
                return L""; // no space for writing, can only read the null value.
            }
        }
    };

    /// @cond
    namespace details
    {
        template<typename string_type> struct string_allocator<unique_string_value<string_type>>
        {
            static void* allocate(size_t size) WI_NOEXCEPT
            {
                return string_allocator<string_type>::allocate(size);
            }
        };
    }
    /// @endcond

#endif

#if defined(__WIL_WINBASE_DESKTOP) && !defined(__WIL_INTERNAL_WINBASE_DESKTOP)
#define __WIL_INTERNAL_WINBASE_DESKTOP
#ifndef WIL_HIDE_DEPRECATED_1611
    WIL_WARN_DEPRECATED_1611_PRAGMA(unique_security_descriptor)

        // WARNING: 'unique_security_descriptor' has been deprecated and is pending deletion!
        // Use:
        // 1. 'unique_hlocal_security_descriptor' if you need to free using 'LocalFree'.
        // 2. 'unique_private_security_descriptor' if you need to free using 'DestroyPrivateObjectSecurity'.

        // DEPRECATED: use unique_private_security_descriptor
        using unique_security_descriptor = unique_private_security_descriptor;
#endif
#endif // __WIL_INTERNAL_WINBASE_DESKTOP

} // namespace wil

#pragma warning(pop)
