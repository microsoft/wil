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
#ifndef __WIL_REGISTRY_HELPERS_INCLUDED
#define __WIL_REGISTRY_HELPERS_INCLUDED

#if defined(_STRING_) || defined (_VECTOR_) || (defined (__cpp_lib_optional) && defined (_OPTIONAL_))
#include <functional>
#include <iterator>
#endif

#include <Windows.h>
#include "resource.h"

#ifdef _KERNEL_MODE
#error This header is not supported in kernel-mode.
#endif

#ifndef __WIL_WINREG_
#error This is required for ::wil::unique_hkey support
#endif

namespace wil
{
    namespace reg
    {
        enum class key_access
        {
            read,
            readwrite,
        };

        namespace reg_view_details
        {
            constexpr DWORD get_value_flags_from_value_type(DWORD type) WI_NOEXCEPT
            {
                switch (type)
                {
                case REG_DWORD:
                    return RRF_RT_REG_DWORD;
                case REG_QWORD:
                    return RRF_RT_REG_QWORD;
                case REG_SZ:
                    return RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND;
                case REG_EXPAND_SZ:
                    return RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ;
                case REG_MULTI_SZ:
                    return RRF_RT_REG_MULTI_SZ;
                case REG_BINARY:
                    return RRF_RT_REG_BINARY;
                    // the caller can directly specify their own flags if they need to
                default:
                    return type;
                }
            }

            constexpr DWORD get_access_flags(key_access access) WI_NOEXCEPT
            {
                switch (access)
                {
                case key_access::read:
                    return KEY_READ;
                case key_access::readwrite:
                    return KEY_ALL_ACCESS;
                }
                FAIL_FAST();
            }

#if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
            template <class InputIt>
            ::std::vector<wchar_t> get_multistring_from_wstrings(const InputIt& first, const InputIt& last)
            {
                ::std::vector<wchar_t> multistring;

                if (first == last)
                {
                    multistring.push_back(L'\0');
                    multistring.push_back(L'\0');
                    return multistring;
                }

                for (const auto& wstr : ::wil::make_range(first, last))
                {
                    if (!wstr.empty())
                    {
                        // back_inserter returns an iterator to enable ::std::copy to write
                        // from begin() to end() at the back_inserter iterator, growing the vector as necessary
                        ::std::copy(::std::begin(wstr), ::std::end(wstr), ::std::back_inserter(multistring));
                    }
                    multistring.push_back(L'\0');
                }
                // double-null-terminate the last string
                multistring.push_back(L'\0');
                return multistring;
            }

            inline ::std::vector<::std::wstring> get_wstring_vector_from_multistring(const ::std::vector<wchar_t>& data)
            {
                ::std::vector<::std::wstring> strings;
                if (data.size() < 3)
                {
                    return { ::std::wstring(data.data()) };
                }

                auto current = data.cbegin();
                const auto end_iterator = data.cend();
                const auto lastNull = (end_iterator - 1);
                while (current != end_iterator)
                {
                    const auto next = ::std::find(current, end_iterator, L'\0');
                    if (next != end_iterator)
                    {
                        if (next != lastNull)
                        {
                            // don't add an empty string for the final 2nd-null-terminator
                            strings.emplace_back(current, next);
                        }
                        current = next + 1;
                    }
                    else
                    {
                        current = next;
                    }
                }
                return strings;
            }
#endif // #if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__SDDL_H__)
            template <typename err_policy>
            ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> create_security_descriptor(_In_opt_ PCWSTR security_descriptor)
            {
                ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> securityDescriptor;
                if (security_descriptor)
                {
                    constexpr PULONG null_StringSecurityDescriptorSize{ nullptr };
                    const auto convert_succeeded = ::ConvertStringSecurityDescriptorToSecurityDescriptorW(
                        security_descriptor,
                        SDDL_REVISION_1,
                        ::wil::out_param_ptr<PSECURITY_DESCRIPTOR*>(securityDescriptor),
                        null_StringSecurityDescriptorSize);
                    err_policy::LastErrorIfFalse(convert_succeeded);
                }
                return securityDescriptor;
            }
#endif // #if defined(__SDDL_H__)

            template <typename T>
            constexpr bool supports_prepare_buffer(T&) noexcept
            {
                return false;
            }

            template <typename T>
            HRESULT prepare_buffer(T&) noexcept
            {
                // no-op in the default case
                return S_OK;
            }

#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
            constexpr bool supports_prepare_buffer(::std::vector<BYTE>&) noexcept
            {
                return true;
            }
            inline HRESULT prepare_buffer(::std::vector<BYTE>& value) WI_NOEXCEPT try
            {
                // resize the initial vector to at least 1 byte
                // this is needed so we can detect when the registry value exists
                // but the value has zero-bytes
                if (value.empty())
                {
                    value.resize(1, 0x00);
                }

                return S_OK;
            }
            CATCH_RETURN()

                inline void* get_buffer(const ::std::vector<BYTE>& buffer) WI_NOEXCEPT
            {
                return const_cast<BYTE*>(buffer.data());
            }
#endif // #if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
            inline void* get_buffer(const ::std::wstring& string) WI_NOEXCEPT
            {
                return const_cast<wchar_t*>(string.data());
            }
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

            constexpr void* get_buffer(const int32_t& value) WI_NOEXCEPT
            {
                return const_cast<int32_t*>(&value);
            }

            constexpr void* get_buffer(const uint32_t& value) WI_NOEXCEPT
            {
                return const_cast<uint32_t*>(&value);
            }

            constexpr void* get_buffer(const long& value) WI_NOEXCEPT
            {
                return const_cast<long*>(&value);
            }

            constexpr void* get_buffer(const unsigned long& value) WI_NOEXCEPT
            {
                return const_cast<unsigned long*>(&value);
            }

            constexpr void* get_buffer(const int64_t& value) WI_NOEXCEPT
            {
                return const_cast<int64_t*>(&value);
            }

            constexpr void* get_buffer(const uint64_t& value) WI_NOEXCEPT
            {
                return const_cast<uint64_t*>(&value);
            }

            constexpr void* get_buffer(PCWSTR value) WI_NOEXCEPT
            {
                return const_cast<wchar_t*>(value);
            }

#if defined(__WIL_OLEAUTO_H_)
            inline void* get_buffer(const BSTR& value) WI_NOEXCEPT
            {
                return value;
            }
            inline void* get_buffer(const ::wil::unique_bstr& value) WI_NOEXCEPT
            {
                return value.get();
            }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
            inline void* get_buffer(const ::wil::unique_cotaskmem_string& value) WI_NOEXCEPT
            {
                return value.get();
            }
#endif // defined(__WIL_OBJBASE_H_)

#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
            inline DWORD get_buffer_size(const ::std::vector<BYTE>& buffer) WI_NOEXCEPT
            {
                return static_cast<DWORD>(buffer.size());
            }
#endif // #if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
            inline DWORD get_buffer_size(const ::std::wstring& string) WI_NOEXCEPT
            {
                return static_cast<DWORD>(string.size() * sizeof(wchar_t));
            }
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

            constexpr DWORD get_buffer_size(int32_t) WI_NOEXCEPT
            {
                return static_cast<DWORD>(sizeof(int32_t));
            }

            constexpr DWORD get_buffer_size(uint32_t) WI_NOEXCEPT
            {
                return static_cast<DWORD>(sizeof(uint32_t));
            }

            constexpr DWORD get_buffer_size(long) WI_NOEXCEPT
            {
                return static_cast<DWORD>(sizeof(long));
            }

            constexpr DWORD get_buffer_size(unsigned long) WI_NOEXCEPT
            {
                return static_cast<DWORD>(sizeof(unsigned long));
            }

            constexpr DWORD get_buffer_size(int64_t) WI_NOEXCEPT
            {
                return static_cast<DWORD>(sizeof(int64_t));
            }

            constexpr DWORD get_buffer_size(uint64_t) WI_NOEXCEPT
            {
                return static_cast<DWORD>(sizeof(uint64_t));
            }

            inline DWORD get_buffer_size(PCWSTR value) WI_NOEXCEPT
            {
                if (!value)
                {
                    return 0;
                }
                return static_cast<DWORD>((::wcslen(value) + 1) * sizeof(wchar_t));
            }

#if defined(__WIL_OLEAUTO_H_)
            inline DWORD get_buffer_size(const BSTR& value) WI_NOEXCEPT
            {
                auto length = ::SysStringLen(value);
                if (length > 0)
                {
                    // SysStringLen does not count the null-terminator
                    length += 1;
                }
                return length * sizeof(wchar_t);
            }

            inline DWORD get_buffer_size(const ::wil::unique_bstr& value) WI_NOEXCEPT
            {
                auto length = ::SysStringLen(value.get());
                if (length > 0)
                {
                    // SysStringLen does not count the null-terminator
                    length += 1;
                }
                return length * sizeof(wchar_t);
            }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
            constexpr DWORD get_buffer_size(const ::wil::unique_cotaskmem_string&) WI_NOEXCEPT
            {
                // wil::unique_cotaskmem_string does not intrinsically track its internal buffer size
                // thus the caller must track the buffer size it requested to be allocated
                return 0;
            }
#endif // defined(__WIL_OBJBASE_H_)


            template <typename T>
            constexpr bool supports_resize_buffer() noexcept
            {
                return false;
            }
            template <typename T>
            constexpr HRESULT resize_buffer(T&, DWORD) WI_NOEXCEPT
            {
                return E_NOTIMPL;
            }

#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
            template <>
            constexpr bool supports_resize_buffer<::std::vector<BYTE>>() noexcept
            {
                return true;
            }
            inline HRESULT resize_buffer(::std::vector<BYTE>& buffer, DWORD byteSize) WI_NOEXCEPT try
            {
                buffer.resize(byteSize);
                return S_OK;
            }
            CATCH_RETURN()
#endif // #if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
                template <>
            constexpr bool supports_resize_buffer<::std::wstring>() noexcept
            {
                return true;
            }
            inline HRESULT resize_buffer(::std::wstring& string, DWORD byteSize) WI_NOEXCEPT try
            {
                string.resize(byteSize / sizeof(wchar_t));
                return S_OK;
            }
            CATCH_RETURN()
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_OLEAUTO_H_)
                template <>
            constexpr bool supports_resize_buffer<BSTR>() noexcept
            {
                return true;
            }
            inline HRESULT resize_buffer(BSTR& string, DWORD byteSize) WI_NOEXCEPT
            {
                // convert back to length (number of WCHARs)
                DWORD length = byteSize / sizeof(WCHAR);
                // SysAllocStringLen adds a null, so subtract a wchar_t from the input byteSize
                length = length > 0 ? length - 1 : length;
                const BSTR newString{ ::SysAllocStringLen(string, length) };
                if (!newString)
                {
                    return E_OUTOFMEMORY;
                }
                ::SysFreeString(string);
                string = newString;
                return S_OK;
            }

            template<>
            constexpr bool supports_resize_buffer<::wil::unique_bstr>() noexcept
            {
                return true;
            }
            inline HRESULT resize_buffer(::wil::unique_bstr& string, DWORD byteSize) WI_NOEXCEPT
            {
                BSTR newBstr = string.release();
                // pass the raw bstr to copy whatever was in the original bstr
                const auto hr = ::wil::reg::reg_view_details::resize_buffer(newBstr, byteSize);

                // if hr failed, put the original bstr back in the unique_bstr because resize_buffer() did not free it
                // if hr succeeded, resize_buffer() freed the bstr we passed it and returned a new bstr
                string.reset(newBstr);
                return hr;
            }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
            template<>
            constexpr bool supports_resize_buffer<::wil::unique_cotaskmem_string>() noexcept
            {
                return true;
            }
            inline HRESULT resize_buffer(::wil::unique_cotaskmem_string& string, DWORD byteSize) WI_NOEXCEPT
            {
                size_t length = byteSize / sizeof(wchar_t);
                // subtracting 1 wchar_t in length because ::wil::make_unique_string_nothrow adds one to the length when it allocates
                length -= 1;
                auto newString = ::wil::make_unique_string_nothrow<::wil::unique_cotaskmem_string>(string.get(), length);
                if (!newString)
                {
                    return E_OUTOFMEMORY;
                }
                string = std::move(newString);
                return S_OK;
            }
#endif // defined(__WIL_OBJBASE_H_)

            template <typename T>
            constexpr bool supports_trim_buffer() WI_NOEXCEPT
            {
                return false;
            }
            template <typename T>
            constexpr void trim_buffer(T&) WI_NOEXCEPT
            {
            }

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
            template <>
            constexpr bool supports_trim_buffer<::std::wstring>() WI_NOEXCEPT
            {
                return true;
            }
            inline void trim_buffer(::std::wstring& buffer)
            {
                const auto offset = buffer.find_first_of(L'\0');
                if (offset != ::std::wstring::npos)
                {
                    buffer.resize(offset);
                }
            }
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

            // get_value_type requires a well-known type for T
            template <typename T>
            DWORD get_value_type() WI_NOEXCEPT;

            template <>
            constexpr DWORD get_value_type<int32_t>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_DWORD);
            }

            template <>
            constexpr DWORD get_value_type<uint32_t>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_DWORD);
            }

            template <>
            constexpr DWORD get_value_type<long>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_DWORD);
            }

            template <>
            constexpr DWORD get_value_type<unsigned long>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_DWORD);
            }

            template <>
            constexpr DWORD get_value_type<int64_t>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_QWORD);
            }

            template <>
            constexpr DWORD get_value_type<uint64_t>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_QWORD);
            }

            template <>
            constexpr DWORD get_value_type<PCWSTR>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_SZ);
            }

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
            template <>
            constexpr DWORD get_value_type<::std::wstring>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_SZ);
            }
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_OLEAUTO_H_)
            template <>
            constexpr DWORD get_value_type<BSTR>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_SZ);
            }
            template <>
            constexpr DWORD get_value_type<::wil::unique_bstr>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_SZ);
            }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
            template <>
            constexpr DWORD get_value_type<::wil::unique_cotaskmem_string>() WI_NOEXCEPT
            {
                return get_value_flags_from_value_type(REG_SZ);
            }
#endif // defined(__WIL_OBJBASE_H_)

            constexpr DWORD set_value_type(int32_t) WI_NOEXCEPT
            {
                return REG_DWORD;
            }

            constexpr DWORD set_value_type(uint32_t) WI_NOEXCEPT
            {
                return REG_DWORD;
            }

            constexpr DWORD set_value_type(long) WI_NOEXCEPT
            {
                return REG_DWORD;
            }

            constexpr DWORD set_value_type(unsigned long) WI_NOEXCEPT
            {
                return REG_DWORD;
            }

            constexpr DWORD set_value_type(int64_t) WI_NOEXCEPT
            {
                return REG_QWORD;
            }

            constexpr DWORD set_value_type(uint64_t) WI_NOEXCEPT
            {
                return REG_QWORD;
            }

            constexpr DWORD set_value_type(PCWSTR) WI_NOEXCEPT
            {
                return REG_SZ;
            }

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
            constexpr DWORD set_value_type(const ::std::wstring&) WI_NOEXCEPT
            {
                return REG_SZ;
            }
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_OLEAUTO_H_)
            constexpr DWORD set_value_type(const BSTR&) WI_NOEXCEPT
            {
                return REG_SZ;
            }
            constexpr DWORD set_value_type(const ::wil::unique_bstr&) WI_NOEXCEPT
            {
                return REG_SZ;
            }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
            constexpr DWORD set_value_type(const ::wil::unique_cotaskmem_string&) WI_NOEXCEPT
            {
                return REG_SZ;
            }
#endif // defined(__WIL_OBJBASE_H_)

            template <typename err_policy = ::wil::err_exception_policy>
            class reg_view_t
            {
            public:
                explicit reg_view_t(HKEY key) WI_NOEXCEPT :
                    m_key(key)
                {
                }
                ~reg_view_t() = default;
                reg_view_t(const reg_view_t&) = delete;
                reg_view_t& operator=(const reg_view_t&) = delete;
                reg_view_t(reg_view_t&&) = delete;
                reg_view_t& operator=(reg_view_t&&) = delete;

                typename err_policy::result open_key(_In_opt_ PCWSTR subKey, _Out_ HKEY* hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    constexpr DWORD zero_ulOptions{ 0 };
                    const auto error = ::RegOpenKeyExW(m_key, subKey, zero_ulOptions, get_access_flags(access), hkey);
                    return err_policy::HResult(HRESULT_FROM_WIN32(error));
                }

                ::wil::unique_hkey open_unique_key(_In_opt_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    ::wil::unique_hkey local_key{};
                    open_key(subKey, &local_key, access);
                    return local_key;
                }

#if defined(__WIL_WINREG_STL)
                ::wil::shared_hkey open_shared_key(_In_opt_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    ::wil::shared_hkey local_key{};
                    open_key(subKey, &local_key, access);
                    return local_key;
                }
#endif // #if defined(__WIL_WINREG_STL)

                typename err_policy::result create_key(PCWSTR subKey, _Out_ HKEY* hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    *hkey = nullptr;

                    constexpr DWORD zero_Reserved{ 0 };
                    constexpr LPWSTR null_lpClass{ nullptr };
                    constexpr DWORD zero_dwOptions{ 0 };
                    constexpr LPSECURITY_ATTRIBUTES null_lpSecurityAttributes{ nullptr };
                    DWORD disposition{ 0 };
                    const auto error =
                        ::RegCreateKeyExW(m_key, subKey, zero_Reserved, null_lpClass, zero_dwOptions, get_access_flags(access), null_lpSecurityAttributes, hkey, &disposition);
                    return err_policy::HResult(HRESULT_FROM_WIN32(error));
                }

                ::wil::unique_hkey create_unique_key(PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    ::wil::unique_hkey local_key{};
                    create_key(subKey, &local_key, access);
                    return local_key;
                }

#if defined(__WIL_WINREG_STL)
                ::wil::shared_hkey create_shared_key(PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    ::wil::shared_hkey local_key{};
                    create_key(subKey, &local_key, access);
                    return local_key;
                }
#endif // #if defined(__WIL_WINREG_STL)

#if defined(__SDDL_H__)
                typename err_policy::result create_key(PCWSTR subKey, _In_opt_ PCWSTR security_descriptor, _Out_ HKEY* hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    *hkey = nullptr;

                    const ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> securityDescriptor = create_security_descriptor<err_policy>(security_descriptor);
                    constexpr LPVOID null_lpSecurityDescriptor{ nullptr };
                    constexpr BOOL false_bInheritHandle{ FALSE };
                    SECURITY_ATTRIBUTES security_attributes{ sizeof security_attributes, null_lpSecurityDescriptor, false_bInheritHandle };
                    security_attributes.lpSecurityDescriptor = securityDescriptor.get();

                    constexpr DWORD zero_Reserved{ 0 };
                    constexpr LPWSTR null_lpClass{ nullptr };
                    constexpr DWORD zero_dwOptions{ 0 };
                    DWORD disposition{ 0 };
                    const auto error =
                        ::RegCreateKeyExW(m_key, subKey, zero_Reserved, null_lpClass, zero_dwOptions, get_access_flags(access), security_descriptor ? &security_attributes : nullptr, hkey, &disposition);
                    return err_policy::HResult(HRESULT_FROM_WIN32(error));
                }

                ::wil::unique_hkey create_unique_key(PCWSTR subKey, _In_opt_ PCWSTR security_descriptor, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    ::wil::unique_hkey local_key{};
                    create_key(subKey, security_descriptor, &local_key, access);
                    return local_key;
                }

#if defined(__WIL_WINREG_STL)
                ::wil::shared_hkey create_shared_key(PCWSTR subKey, _In_opt_ PCWSTR security_descriptor, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    ::wil::shared_hkey local_key{};
                    create_key(subKey, security_descriptor, &local_key, access);
                    return local_key;
                }
#endif // #if defined(__WIL_WINREG_STL)
#endif // #if defined(__SDDL_H__)

                typename err_policy::result delete_key(_In_opt_ PCWSTR sub_key) WI_NOEXCEPT
                {
                    auto error = ::RegDeleteTreeW(m_key, sub_key);
                    if (error == ERROR_FILE_NOT_FOUND)
                    {
                        error = ERROR_SUCCESS;
                    }
                    return err_policy::HResult(HRESULT_FROM_WIN32(error));
                }

                typename err_policy::result delete_value(_In_opt_ PCWSTR value_name) WI_NOEXCEPT
                {
                    return err_policy::HResult(HRESULT_FROM_WIN32(::RegDeleteValueW(m_key, value_name)));
                }

            private:
                template <typename R, typename get_value_internal_policy = err_policy>
                typename get_value_internal_policy::result get_value_internal(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, R& return_value, DWORD type = ::wil::reg::reg_view_details::get_value_type<R>()) const
                {
                    HRESULT hr = S_OK;

                    if
#if defined(__cpp_if_constexpr)
                        constexpr
#endif
                        (::wil::reg::reg_view_details::supports_prepare_buffer(value_name))

                    {
                        hr = ::wil::reg::reg_view_details::prepare_buffer(return_value);
                        if (FAILED(hr))
                        {
                            return get_value_internal_policy::HResult(hr);
                        }
                    }

                    DWORD bytes_allocated{ ::wil::reg::reg_view_details::get_buffer_size(return_value) };
                    for (;;)
                    {
                        constexpr LPDWORD null_pdwType{ nullptr };
                        DWORD data_size_bytes{ bytes_allocated };
                        hr = HRESULT_FROM_WIN32(::RegGetValueW(
                            m_key,
                            subkey,
                            value_name,
                            get_value_flags_from_value_type(type),
                            null_pdwType,
                            ::wil::reg::reg_view_details::get_buffer(return_value),
                            &data_size_bytes));

                        // some return types we can grow as needed - e.g. when writing to a std::wstring
                        // will only compile the below for those types that support dynamically growing the buffer
                        if
#if defined(__cpp_if_constexpr)
                            constexpr
#endif
                            (::wil::reg::reg_view_details::supports_resize_buffer<R>())
                        {
                            // GetRegValueW will indicate the caller allocate the returned number of bytes in one of two cases:
                            // 1. returns ERROR_MORE_DATA
                            // 2. returns ERROR_SUCCESS when we gave it a nullptr for the out buffer
                            const bool shouldReallocate = (hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA)) || (SUCCEEDED(hr) && (::wil::reg::reg_view_details::get_buffer(return_value) == nullptr) && (data_size_bytes > 0));
                            if (shouldReallocate)
                            {
                                // verify if resize_buffer succeeded allocation
                                hr = ::wil::reg::reg_view_details::resize_buffer(return_value, data_size_bytes);
                                if (SUCCEEDED(hr))
                                {
                                    // if it succeeds, continue the for loop to try again
                                    bytes_allocated = data_size_bytes;
                                    continue;
                                }
                            }

                            if (SUCCEEDED(hr))
                            {
                                const auto current_byte_size = ::wil::reg::reg_view_details::get_buffer_size(return_value);
                                if (current_byte_size != data_size_bytes)
                                {
                                    hr = ::wil::reg::reg_view_details::resize_buffer(return_value, data_size_bytes);
                                }
                            }

                            if (SUCCEEDED(hr))
                            {
                                if
#if defined(__cpp_if_constexpr)
                                    constexpr
#endif
                                    (::wil::reg::reg_view_details::supports_trim_buffer<R>())

                                {
                                    ::wil::reg::reg_view_details::trim_buffer(return_value);
                                }
                            }
                        }
                        break;
                    }


                    return get_value_internal_policy::HResult(hr);
                }

            public:
                template <typename R>
                typename err_policy::result get_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, R& return_value, DWORD type = ::wil::reg::reg_view_details::get_value_type<R>()) const
                {
                    return get_value_internal(subkey, value_name, return_value, type);
                }

                template <typename R>
                typename err_policy::result get_value(_In_opt_ PCWSTR value_name, R& return_value, DWORD type = ::wil::reg::reg_view_details::get_value_type<R>()) const
                {
                    return get_value_internal(nullptr, value_name, return_value, type);
                }

#if defined (_OPTIONAL_) && defined(__cpp_lib_optional)
                // intended for err_exception_policy as err_returncode_policy will not get an error code
                template <typename R>
                ::std::optional<R> try_get_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type = ::wil::reg::reg_view_details::get_value_type<R>()) const
                {
                    R value{};
                    const auto hr = get_value_internal<R, ::wil::err_returncode_policy>(subkey, value_name, value, type);
                    if (SUCCEEDED(hr))
                    {
                        return std::optional(std::move(value));
                    }

                    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                    {
                        return ::std::nullopt;
                    }

                    // throw if exception policy
                    err_policy::HResult(HRESULT_FROM_WIN32(hr));
                    return ::std::nullopt;
                }

                template <typename R>
                ::std::optional<R>& try_get_value(_In_opt_ PCWSTR value_name, DWORD type = ::wil::reg::reg_view_details::get_value_type<R>()) const
                {
                    return try_get_value<R>(nullptr, value_name, type);
                }
#endif // #if defined (_OPTIONAL_) && defined(__cpp_lib_optional)

                template <typename R>
                typename err_policy::result set_value_with_type(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const R& value, DWORD type) const
                {
                    const auto error = ::RegSetKeyValueW(
                        m_key,
                        subkey,
                        value_name,
                        type,
                        static_cast<BYTE*>(::wil::reg::reg_view_details::get_buffer(value)),
                        ::wil::reg::reg_view_details::get_buffer_size(value));
                    return err_policy::HResult(HRESULT_FROM_WIN32(error));
                }

                template <typename R>
                typename err_policy::result set_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const R& value) const
                {
                    return set_value_with_type(subkey, value_name, value, ::wil::reg::reg_view_details::set_value_type(value));
                }

                template <typename R>
                typename err_policy::result set_value(_In_opt_ PCWSTR value_name, const R& value) const
                {
                    return set_value(nullptr, value_name, value);
                }

#if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
                template <typename R>
                typename err_policy::result set_value_multistring(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const R& data) const try
                {
                    const auto multiStringWcharVector(get_multistring_from_wstrings(::std::begin(data), ::std::end(data)));

                    const auto* byteArray = reinterpret_cast<const BYTE*>(&multiStringWcharVector[0]);
                    const auto byteArrayLength = static_cast<DWORD>(multiStringWcharVector.size() * sizeof(wchar_t));
                    const auto error = ::RegSetKeyValueW(
                        m_key,
                        subkey,
                        value_name,
                        REG_MULTI_SZ,
                        byteArray,
                        byteArrayLength);
                    return err_policy::HResult(HRESULT_FROM_WIN32(error));
                }
                catch (...)
                {
                    return err_policy::HResult(::wil::ResultFromCaughtException());
                }

                template <typename R>
                typename err_policy::result set_value_multistring(_In_opt_ PCWSTR value_name, const R& data) const
                {
                    return set_value_multistring(nullptr, value_name, data);
                }
#endif // #if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
            private:
                const HKEY m_key{};
            };

            // reg_view with the raw HKEY type is non-owning
            using reg_view_nothrow = reg_view_details::reg_view_t<::wil::err_returncode_policy>;

#if defined(WIL_ENABLE_EXCEPTIONS)
            // reg_view with the raw HKEY type is non-owning
            using reg_view = reg_view_details::reg_view_t<::wil::err_exception_policy>;
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)
        }
    } // namespace reg
} // namespace wil
#endif // __WIL_REGISTRY_HELPERS_INCLUDED