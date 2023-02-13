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
#ifndef __WIL_REGISTRY_BASIC_INCLUDED
#define __WIL_REGISTRY_BASIC_INCLUDED

// wil registry does not require the use of the STL - though it does natively support std::vector and std::wstring
// wil registry uses the __WIL_WINREG_STL define to track support for wil::shared_* types (defined in resource.h)
// Include <string> and/or <vector> to use the STL integrated functions
// Include sddl.h if wanting to pass security descriptor strings to create keys

#if defined(_STRING_) || defined (_VECTOR_) || (defined (__cpp_lib_optional) && defined (_OPTIONAL_))
#include <functional>
#include <iterator>
#endif

#include <Windows.h>

#include "common.h"
#include "resource.h"

#ifdef _KERNEL_MODE
#error This header is not supported in kernel-mode.
#endif

#ifndef __WIL_WINREG_
#error This required ::wil::unique_hkey support
#endif

namespace wil
{
    namespace reg
    {
        constexpr bool is_hresult_not_found(HRESULT hr) WI_NOEXCEPT
        {
            return hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        constexpr bool is_hresult_buffer_too_small(HRESULT hr) WI_NOEXCEPT
        {
            return hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA);
        }

        enum class key_access
        {
            read,
            readwrite,
        };

        namespace details
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
                    return strings;
                }

                auto current = data.cbegin();
                const auto end_iterator = data.cend();
                const auto lastNull = (end_iterator - 1);
                while (current != end_iterator)
                {
                    const auto next = ::std::find(current, data.cend(), L'\0');
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
        } // namespace details

        namespace reg_view_details
        {
#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
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

            constexpr void* get_buffer(_In_ PCWSTR value) WI_NOEXCEPT
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

            //
            // get_buffer_size is needed to call RegGetValue internally
            // it can return one of 3 values:
            // - if known, the already-allocated size of the buffer passed
            // - zero, if nothing has been allocated
            // - negative one if the type does not track its own buffer intrinsically
            //
            constexpr DWORD untracked_buffer_size = 0xffffffff;
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

            inline DWORD get_buffer_size(_In_ PCWSTR value) WI_NOEXCEPT
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
                // SysStringLen does not count the null-terminator
                return (::SysStringLen(value) + 1) * sizeof(wchar_t);
            }

            inline DWORD get_buffer_size(const ::wil::unique_bstr& value) WI_NOEXCEPT
            {
                // SysStringLen does not count the null-terminator
                return (::SysStringLen(value.get()) + 1) * sizeof(wchar_t);
            }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
            inline DWORD get_buffer_size(const ::wil::unique_cotaskmem_string&) WI_NOEXCEPT
            {
                // wil::unique_cotaskmem_string does not intrinsically track its internal buffer size
                // thus the caller must track the buffer size it requested to be allocated
                return ::wil::reg::reg_view_details::untracked_buffer_size;
            }
#endif // defined(__WIL_OBJBASE_H_)

            // grow_buffer_if_supported returns:
            // - true if growing that buffer type is supported and the allocation succeeded
            // - false if growing that type is not supported or the allocation failed
            template <typename T>
            constexpr bool grow_buffer_if_supported(T&, DWORD) WI_NOEXCEPT
            {
                return false;
            }

#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
            inline bool grow_buffer_if_supported(::std::vector<BYTE>& buffer, DWORD byteSize) WI_NOEXCEPT
            {
                try
                {
                    buffer.resize(byteSize);
                }
                catch (...)
                {
                    return false;
                }
                return true;
            }
#endif // #if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
            inline bool grow_buffer_if_supported(::std::wstring& string, DWORD byteSize) WI_NOEXCEPT
            {
                try
                {
                    string.resize(byteSize / sizeof(wchar_t));
                }
                catch (...)
                {
                    return false;
                }
                return true;
            }
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_OLEAUTO_H_)
            inline bool grow_buffer_if_supported(BSTR& string, DWORD byteSize) WI_NOEXCEPT
            {
                // convert back to length (number of WCHARs)
                DWORD length = byteSize / sizeof(WCHAR);
                // SysAllocStringLen adds a null, so subtract a wchar_t from the input byteSize
                length = length > 0 ? length - 1 : length;
                const BSTR newString{ ::SysAllocStringLen(string, length) };
                if (!newString)
                {
                    return false;
                }
                string = newString;
                return true;
            }
            inline bool grow_buffer_if_supported(::wil::unique_bstr& string, DWORD byteSize) WI_NOEXCEPT
            {
                BSTR newBstr = string.get();
                if (::wil::reg::reg_view_details::grow_buffer_if_supported(newBstr, byteSize))
                {
                    string.reset(newBstr);
                    return true;
                }
                return false;
            }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
            inline bool grow_buffer_if_supported(::wil::unique_cotaskmem_string& string, DWORD byteSize) WI_NOEXCEPT
            {
                size_t length = byteSize / sizeof(wchar_t);
                // subtracting 1 wchar_t in length because ::wil::make_unique_string_nothrow adds one to the length when it allocates
                length -= 1;
                auto newString = ::wil::make_unique_string_nothrow<::wil::unique_cotaskmem_string>(string.get(), length);
                if (!newString)
                {
                    return false;
                }
                string = std::move(newString);
                return true;
            }
#endif // defined(__WIL_OBJBASE_H_)

            template <typename T>
            constexpr void trim_buffer(T&) WI_NOEXCEPT
            {
            }

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
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
                return ::wil::reg::details::get_value_flags_from_value_type(REG_DWORD);
            }

            template <>
            constexpr DWORD get_value_type<uint32_t>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_DWORD);
            }

            template <>
            constexpr DWORD get_value_type<long>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_DWORD);
            }

            template <>
            constexpr DWORD get_value_type<unsigned long>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_DWORD);
            }

            template <>
            constexpr DWORD get_value_type<int64_t>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_QWORD);
            }

            template <>
            constexpr DWORD get_value_type<uint64_t>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_QWORD);
            }

            template <>
            constexpr DWORD get_value_type<PCWSTR>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_SZ);
            }

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
            template <>
            constexpr DWORD get_value_type<::std::wstring>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_SZ);
            }
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_OLEAUTO_H_)
            template <>
            constexpr DWORD get_value_type<BSTR>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_SZ);
            }
            template <>
            constexpr DWORD get_value_type<::wil::unique_bstr>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_SZ);
            }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
            template <>
            inline DWORD get_value_type<::wil::unique_cotaskmem_string>() WI_NOEXCEPT
            {
                return ::wil::reg::details::get_value_flags_from_value_type(REG_SZ);
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
            inline DWORD set_value_type(const ::wil::unique_cotaskmem_string&) WI_NOEXCEPT
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
                    const auto error = ::RegOpenKeyExW(m_key, subKey, zero_ulOptions, ::wil::reg::details::get_access_flags(access), hkey);
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

                typename err_policy::result create_key(_In_ PCWSTR subKey, _Out_ HKEY* hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    *hkey = nullptr;

                    constexpr DWORD zero_Reserved{ 0 };
                    constexpr LPWSTR null_lpClass{ nullptr };
                    constexpr DWORD zero_dwOptions{ 0 };
                    constexpr LPSECURITY_ATTRIBUTES null_lpSecurityAttributes{ nullptr };
                    DWORD disposition{ 0 };
                    const auto error =
                        ::RegCreateKeyExW(m_key, subKey, zero_Reserved, null_lpClass, zero_dwOptions, ::wil::reg::details::get_access_flags(access), null_lpSecurityAttributes, hkey, &disposition);
                    return err_policy::HResult(HRESULT_FROM_WIN32(error));
                }

                ::wil::unique_hkey create_unique_key(_In_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    ::wil::unique_hkey local_key{};
                    create_key(subKey, &local_key, access);
                    return local_key;
                }

#if defined(__WIL_WINREG_STL)
                ::wil::shared_hkey create_shared_key(_In_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    ::wil::shared_hkey local_key{};
                    create_key(subKey, &local_key, access);
                    return local_key;
                }
#endif // #if defined(__WIL_WINREG_STL)

#if defined(__SDDL_H__)
                typename err_policy::result create_key(_In_ PCWSTR subKey, _In_opt_ PCWSTR security_descriptor, _Out_ HKEY* hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    *hkey = nullptr;

                    const ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> securityDescriptor = ::wil::reg::details::create_security_descriptor<err_policy>(security_descriptor);
                    constexpr LPVOID null_lpSecurityDescriptor{ nullptr };
                    constexpr BOOL false_bInheritHandle{ FALSE };
                    SECURITY_ATTRIBUTES security_attributes{ sizeof security_attributes, null_lpSecurityDescriptor, false_bInheritHandle };
                    security_attributes.lpSecurityDescriptor = securityDescriptor.get();

                    constexpr DWORD zero_Reserved{ 0 };
                    constexpr LPWSTR null_lpClass{ nullptr };
                    constexpr DWORD zero_dwOptions{ 0 };
                    DWORD disposition{ 0 };
                    const auto error =
                        ::RegCreateKeyExW(m_key, subKey, zero_Reserved, null_lpClass, zero_dwOptions, ::wil::reg::details::get_access_flags(access), security_descriptor ? &security_attributes : nullptr, hkey, &disposition);
                    return err_policy::HResult(HRESULT_FROM_WIN32(error));
                }

                ::wil::unique_hkey create_unique_key(_In_ PCWSTR subKey, _In_opt_ PCWSTR security_descriptor, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
                {
                    ::wil::unique_hkey local_key{};
                    create_key(subKey, security_descriptor, &local_key, access);
                    return local_key;
                }

#if defined(__WIL_WINREG_STL)
                ::wil::shared_hkey create_shared_key(_In_ PCWSTR subKey, _In_opt_ PCWSTR security_descriptor, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
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
                    DWORD bytes_allocated{};
                    for (;;)
                    {
                        constexpr LPDWORD null_pdwType{ nullptr };
                        DWORD data_size_bytes{ ::wil::reg::reg_view_details::get_buffer_size(return_value) };
                        if (data_size_bytes == ::wil::reg::reg_view_details::untracked_buffer_size)
                        {
                            data_size_bytes = bytes_allocated;
                        }
                        auto error = ::RegGetValueW(
                            m_key,
                            subkey,
                            value_name,
                            ::wil::reg::details::get_value_flags_from_value_type(type),
                            null_pdwType,
                            ::wil::reg::reg_view_details::get_buffer(return_value),
                            &data_size_bytes);

                        // GetRegValueW will indicate the caller allocate the returned number of bytes in one of two cases:
                        // 1. returns ERROR_MORE_DATA
                        // 2. returns ERROR_SUCCESS when we gave it a nullptr for the out buffer
                        const bool shouldReallocate = (error == ERROR_MORE_DATA) || (error == ERROR_SUCCESS && ::wil::reg::reg_view_details::get_buffer(return_value) == nullptr);
                        if (shouldReallocate)
                        {
                            // verify if grow_buffer_if_supported succeeded allocation
                            if (!::wil::reg::reg_view_details::grow_buffer_if_supported(return_value, data_size_bytes))
                            {
                                // will return this error after the if block
                                error = ERROR_NOT_ENOUGH_MEMORY;
                            }
                            else
                            {
                                // if it succeeds, continue the for loop to try again
                                bytes_allocated = data_size_bytes;
                                continue;
                            }
                        }

                        if (error == ERROR_SUCCESS)
                        {
                            const auto current_byte_size = ::wil::reg::reg_view_details::get_buffer_size(return_value);
                            if (current_byte_size != ::wil::reg::reg_view_details::untracked_buffer_size && current_byte_size != data_size_bytes)
                            {
                                ::wil::reg::reg_view_details::grow_buffer_if_supported(return_value, data_size_bytes);
                            }
                            break;
                        }

                        // all other errors, including error == ERROR_FILE_NOT_FOUND, are returned to the caller
                        // note that ERROR_MORE_DATA will never be returned - the loop handles that
                        return get_value_internal_policy::HResult(HRESULT_FROM_WIN32(error));
                    }

                    // breaking out of the for loop == successfully read the value
                    ::wil::reg::reg_view_details::trim_buffer(return_value);
                    return get_value_internal_policy::OK();
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
                    const auto multiStringWcharVector(::wil::reg::details::get_multistring_from_wstrings(::std::begin(data), ::std::end(data)));

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

        //
        // open_*_key functions (throwing)
        //
#if defined(WIL_ENABLE_EXCEPTIONS)
        inline ::wil::unique_hkey open_unique_key(HKEY key, _In_opt_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.open_unique_key(path, access);
        }

#if defined(__WIL_WINREG_STL)
        inline ::wil::shared_hkey open_shared_key(HKEY key, _In_opt_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.open_shared_key(path, access);
        }
#endif // #if defined(__WIL_WINREG_STL)

        //
        // create_*_key functions (throwing)
        //
        inline ::wil::unique_hkey create_unique_key(HKEY key, _In_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.create_unique_key(path, access);
        }

#if defined(__SDDL_H__)
        inline ::wil::unique_hkey create_unique_key(HKEY key, _In_ PCWSTR path, _In_opt_ PCWSTR security_descriptor, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.create_unique_key(path, security_descriptor, access);
        }
#endif // #if defined(__SDDL_H__)

#if defined(__WIL_WINREG_STL)
        inline ::wil::shared_hkey create_shared_key(HKEY key, _In_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.create_shared_key(path, access);
        }

#if defined(__SDDL_H__)
        inline ::wil::shared_hkey create_shared_key(HKEY key, _In_ PCWSTR path, _In_opt_ PCWSTR security_descriptor, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.create_shared_key(path, security_descriptor, access);
        }
#endif // #if defined(__SDDL_H__)
#endif // #if defined(__WIL_WINREG_STL)
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

        //
        // open_key_nothrow functions
        //
        inline HRESULT open_key_nothrow(HKEY key, _In_opt_ PCWSTR path, _Out_ HKEY* hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.open_key(path, hkey, access);
        }

        //
        // create_key_nothrow functions
        //
        inline HRESULT create_key_nothrow(HKEY key, _In_ PCWSTR path, _Out_ HKEY* hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.create_key(path, hkey, access);
        }

#if defined(__SDDL_H__)
        inline HRESULT create_key_nothrow(HKEY key, _In_ PCWSTR path, _In_opt_ PCWSTR security_descriptor, _Out_ HKEY* hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.create_key(path, security_descriptor, hkey, access);
        }
#endif // #if defined(__SDDL_H__)


#if defined(WIL_ENABLE_EXCEPTIONS)
        inline size_t get_child_key_count(HKEY key)
        {
            DWORD numSubKeys{};
            THROW_IF_WIN32_ERROR(::RegQueryInfoKeyW(
                key, nullptr, nullptr, nullptr, &numSubKeys, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
            return numSubKeys;
        }

        inline size_t get_child_value_count(HKEY key)
        {
            DWORD numSubValues{};
            THROW_IF_WIN32_ERROR(::RegQueryInfoKeyW(
                key, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, &numSubValues, nullptr, nullptr, nullptr, nullptr));
            return numSubValues;
        }
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

        inline HRESULT get_child_key_count_nothrow(HKEY key, _Out_ DWORD* numSubKeys) WI_NOEXCEPT
        {
            RETURN_IF_WIN32_ERROR(::RegQueryInfoKeyW(
                key, nullptr, nullptr, nullptr, numSubKeys, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
            return S_OK;
        }

        inline HRESULT get_child_value_count_nothrow(HKEY key, _Out_ DWORD* numSubValues) WI_NOEXCEPT
        {
            RETURN_IF_WIN32_ERROR(::RegQueryInfoKeyW(
                key, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, numSubValues, nullptr, nullptr, nullptr, nullptr));
            return S_OK;
        }

        //
        // set_value* (throwing) functions
        //
#if defined(WIL_ENABLE_EXCEPTIONS)
        template <typename T>
        void set_value(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const T& data)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.set_value(subkey, value_name, data);
        }

        template <typename T>
        void set_value(HKEY key, _In_opt_ PCWSTR value_name, const T& data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }

        inline void set_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD data)
        {
            return ::wil::reg::set_value(key, subkey, value_name, data);
        }

        inline void set_value_dword(HKEY key, _In_opt_ PCWSTR value_name, DWORD data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }

        inline void set_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint64_t data)
        {
            return ::wil::reg::set_value(key, subkey, value_name, data);
        }

        inline void set_value_qword(HKEY key, _In_opt_ PCWSTR value_name, uint64_t data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }

        inline void set_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ PCWSTR data)
        {
            return ::wil::reg::set_value(key, subkey, value_name, data);
        }

        inline void set_value_string(HKEY key, _In_opt_ PCWSTR value_name, _In_ PCWSTR data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }

        inline void set_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view regview{ key };
            return regview.set_value_with_type(subkey, value_name, data, REG_EXPAND_SZ);
        }

        inline void set_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_expanded_string(key, nullptr, value_name, data);
        }
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

#if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        inline void set_value_multistring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.set_value_multistring(subkey, value_name, data);
        }

        inline void set_value_multistring(HKEY key, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
        {
            return ::wil::reg::set_value_multistring(key, nullptr, value_name, data);
        }

        template <>
        inline void set_value(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
        {
            return ::wil::reg::set_value_multistring(key, subkey, value_name, data);
        }
        template <>
        inline void set_value(HKEY key, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
        {
            return ::wil::reg::set_value_multistring(key, nullptr, value_name, data);
        }
#endif // #if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

        //
        // set_value_*_nothrow functions
        //
        template <typename T>
        HRESULT set_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const T& data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.set_value(subkey, value_name, data);
        }

        template <typename T>
        HRESULT set_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, const T& data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }

#if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        template <>
        inline HRESULT set_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.set_value_multistring(subkey, value_name, data);
        }

        template <>
        inline HRESULT set_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }
#endif // #if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

        inline HRESULT set_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, subkey, value_name, data);
        }

        inline HRESULT set_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, DWORD data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }

        inline HRESULT set_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint64_t data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, subkey, value_name, data);
        }

        inline HRESULT set_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, uint64_t data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }

        inline HRESULT set_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, subkey, value_name, data);
        }

        inline HRESULT set_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }

        inline HRESULT set_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.set_value_with_type(subkey, value_name, data, REG_EXPAND_SZ);
        }

        inline HRESULT set_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_expanded_string_nothrow(key, nullptr, value_name, data);
        }

#if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        inline HRESULT set_value_multistring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_ PCWSTR value_name, const ::std::vector<::std::wstring>& data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.set_value_multistring(subkey, value_name, data);
        }

        inline HRESULT set_value_multistring_nothrow(HKEY key, _In_ PCWSTR value_name, const ::std::vector<::std::wstring>& data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_multistring_nothrow(key, nullptr, value_name, data);
        }
#endif // #if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

        //
        // get_value* (throwing) functions
        //
#if defined(WIL_ENABLE_EXCEPTIONS)
        template <typename T>
        T get_value(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            T return_value{};
            regview.get_value<T>(subkey, value_name, return_value);
            return return_value;
        }

        template <typename T>
        T get_value(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<T>(key, nullptr, value_name);
        }

        inline DWORD get_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<DWORD>(key, subkey, value_name);
        }

        inline DWORD get_value_dword(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<DWORD>(key, nullptr, value_name);
        }

        inline DWORD64 get_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<DWORD64>(key, subkey, value_name);
        }

        inline DWORD64 get_value_qword(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<DWORD64>(key, nullptr, value_name);
        }

#if defined(_STRING_)
        inline ::std::wstring get_value_wstring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<::std::wstring>(key, subkey, value_name);
        }

        inline ::std::wstring get_value_wstring(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<::std::wstring>(key, nullptr, value_name);
        }

        inline ::std::wstring get_value_expanded_wstring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            ::std::wstring value;
            const reg_view_details::reg_view regview{ key };
            regview.get_value<::std::wstring>(subkey, value_name, value, REG_EXPAND_SZ);
            return value;
        }

        inline ::std::wstring get_value_expanded_wstring(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return get_value_expanded_wstring(key, nullptr, value_name);
        }
#endif // #if defined(_STRING_)

#if defined(__WIL_OLEAUTO_H_)
        inline ::wil::unique_bstr get_value_bstr(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<::wil::unique_bstr>(key, subkey, value_name);
        }

        inline ::wil::unique_bstr get_value_bstr(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<::wil::unique_bstr>(key, nullptr, value_name);
        }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
        inline ::wil::unique_cotaskmem_string get_value_cotaskmem_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<::wil::unique_cotaskmem_string>(key, subkey, value_name);
        }

        inline ::wil::unique_cotaskmem_string get_value_cotaskmem_string(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value<::wil::unique_cotaskmem_string>(key, nullptr, value_name);
        }
#endif // defined(__WIL_OBJBASE_H_)

#if defined(_VECTOR_)
        inline ::std::vector<BYTE> get_value_byte_vector(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type)
        {
            ::std::vector<BYTE> return_value{};
            const reg_view_details::reg_view regview{ key };
            regview.get_value<::std::vector<BYTE>>(subkey, value_name, return_value, type);
            return return_value;
        }

        inline ::std::vector<BYTE> get_value_byte_vector(HKEY key, _In_opt_ PCWSTR value_name, DWORD type)
        {
            return get_value_byte_vector(key, nullptr, value_name, type);
        }
#endif // #if defined(_VECTOR_)

        //
        // try_get_value* (throwing) functions
        //
#if defined (_OPTIONAL_) && defined(__cpp_lib_optional)
        template <typename T>
        ::std::optional<T> try_get_value(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<T>(subkey, value_name);
        }

        inline ::std::optional<DWORD> try_get_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::try_get_value<DWORD>(key, subkey, value_name);
        }

        inline ::std::optional<DWORD> try_get_value_dword(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::try_get_value<DWORD>(key, nullptr, value_name);
        }

        inline ::std::optional<DWORD64> try_get_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::try_get_value<DWORD64>(key, subkey, value_name);
        }

        inline ::std::optional<DWORD64> try_get_value_qword(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::try_get_value<DWORD64>(key, nullptr, value_name);
        }

#if defined(_VECTOR_)
        inline ::std::optional<::std::vector<BYTE>> try_get_value_byte_vector(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::std::vector<BYTE>>(subkey, value_name, type);
        }

        inline ::std::optional<::std::vector<BYTE>> try_get_value_byte_vector(HKEY key, _In_opt_ PCWSTR value_name, DWORD type)
        {
            return try_get_value_byte_vector(key, nullptr, value_name, type);
        }
#endif // #if defined(_VECTOR_)


#if defined(_STRING_)
        inline ::std::optional<::std::wstring> try_get_value_wstring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::std::wstring>(subkey, value_name);
        }

        inline ::std::optional<::std::wstring> try_get_value_wstring(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return try_get_value_wstring(key, nullptr, value_name);
        }

        inline ::std::optional<::std::wstring> try_get_value_expanded_wstring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::std::wstring>(subkey, value_name, REG_EXPAND_SZ);
    }

        inline ::std::optional<::std::wstring> try_get_value_expanded_wstring(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return try_get_value_expanded_wstring(key, nullptr, value_name);
        }
#endif // #if defined(_STRING_)

#if defined(__WIL_OLEAUTO_H_)
        inline ::std::optional<::wil::unique_bstr> try_get_value_bstr(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::wil::unique_bstr>(subkey, value_name);
        }

        inline ::std::optional<::wil::unique_bstr> try_get_value_bstr(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return try_get_value_bstr(key, nullptr, value_name);
        }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
        inline ::std::optional<::wil::unique_cotaskmem_string> try_get_value_cotaskmem_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::wil::unique_cotaskmem_string>(subkey, value_name);
        }

        inline ::std::optional<::wil::unique_cotaskmem_string> try_get_value_cotaskmem_string(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return try_get_value_cotaskmem_string(key, nullptr, value_name);
        }
#endif // defined(__WIL_OBJBASE_H_)
#endif // #if defined (_OPTIONAL_) && defined(__cpp_lib_optional)
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

        //
        // get_value_*_nothrow (throwing) functions
        //
        template <size_t Length>
        HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* pRequiredBytes = nullptr) WI_NOEXCEPT
        {
            // don't allocate, just use their buffer
            wil::assign_to_opt_param(pRequiredBytes, 0ul);
            DWORD data_size_bytes{ Length * sizeof(WCHAR) };
            const auto error = ::RegGetValueW(key, subkey, value_name, ::wil::reg::details::get_value_flags_from_value_type(REG_SZ), nullptr, return_value, &data_size_bytes);
            if (error == ERROR_SUCCESS || error == ERROR_MORE_DATA)
            {
                wil::assign_to_opt_param(pRequiredBytes, data_size_bytes);
            }
            return HRESULT_FROM_WIN32(error);
        }

        template <size_t Length>
        HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* pRequiredBytes = nullptr) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_string_nothrow<Length>(key, nullptr, value_name, return_value, pRequiredBytes);
        }

        // The below  < std::enable_if<!std::is_same<T, wchar_t>::value>::type* = nullptr >
        // allows the correct get_value_nothrow both when passed a DWORD*, as well as when passed a WCHAR[] array
        //
        // e.g. both of the below work correctly
        // 
        // WCHAR data1[100]{};
        // get_value(hkey, L"valuename", data);
        // //
        // DWORD data2{};
        // get_value_nothrow(hkey, L"valuename", &data2);
        template<typename T,
            std::enable_if_t<!std::is_same_v<T, wchar_t>>* = nullptr>
        HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Out_ T* return_value) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value<T>(subkey, value_name, *return_value);
        }

        template<typename T,
            std::enable_if_t<!std::is_same_v<T, wchar_t>>* = nullptr>
        HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Out_ T* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }

        template <size_t Length>
        HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length]) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_string_nothrow<Length>(key, subkey, value_name, return_value, nullptr);
        }

        template <size_t Length>
        HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length]) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_string_nothrow<Length>(key, nullptr, value_name, return_value, nullptr);
        }

        // ::wil::unique_cotaskmem_string requires its own separate get_value_nothrow because it is forced to pass its out-param by reference
        // this is because wil::unique_cotaskmem_string has overloaded operator&
        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, ::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value<::wil::unique_cotaskmem_string>(subkey, value_name, return_value);
        }

        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, ::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }

        inline HRESULT get_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Out_ DWORD* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value);
        }

        inline HRESULT get_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Out_ DWORD* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }

        inline HRESULT get_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Out_ DWORD64* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value);
        }

        inline HRESULT get_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Out_ DWORD64* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        inline HRESULT get_value_wstring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_ ::std::wstring* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value);
        }

        inline HRESULT get_value_wstring_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_ ::std::wstring* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }
#endif

#if defined(__WIL_OLEAUTO_H_)
        inline HRESULT get_value_bstr_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Out_ BSTR* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value);
        }

        inline HRESULT get_value_bstr_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Out_ BSTR* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
        inline HRESULT get_value_cotaskmem_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_ ::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value);
        }
        inline HRESULT get_value_cotaskmem_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }
#endif // defined(__WIL_OBJBASE_H_)

#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
        inline HRESULT get_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type, _Inout_ ::std::vector<BYTE>* data) WI_NOEXCEPT try
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            ::std::vector<BYTE> value{};
            RETURN_IF_FAILED(regview.get_value<::std::vector<BYTE>>(subkey, value_name, value, type));

            data->swap(value);
            return S_OK;
        }
        CATCH_RETURN()

        inline HRESULT get_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR value_name, DWORD type, _Inout_ ::std::vector<BYTE>* data) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_byte_vector_nothrow(key, nullptr, value_name, type, data);
        }
#endif // #if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)

        //
        // get_value_expanded_string_nothrow (throwing) functions
        //
        template <size_t Length>
        HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* pRequiredBytes = nullptr) WI_NOEXCEPT
        {
            // don't allocate, just use their buffer
            wil::assign_to_opt_param(pRequiredBytes, 0ul);
            DWORD data_size_bytes{ Length * sizeof(WCHAR) };
            const auto error = ::RegGetValueW(key, subkey, value_name, ::wil::reg::details::get_value_flags_from_value_type(REG_EXPAND_SZ), nullptr, return_value, &data_size_bytes);
            if (error == ERROR_SUCCESS || error == ERROR_MORE_DATA)
            {
                wil::assign_to_opt_param(pRequiredBytes, data_size_bytes);
            }
            return HRESULT_FROM_WIN32(error);
        }

        template <size_t Length>
        HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* pRequiredBytes = nullptr) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_expanded_string_nothrow<Length>(key, nullptr, value_name, return_value, pRequiredBytes);
        }

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        inline HRESULT get_value_expanded_wstring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_ ::std::wstring* data) WI_NOEXCEPT try
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            std::wstring value{};
            RETURN_IF_FAILED(regview.get_value<::std::wstring>(subkey, value_name, value, REG_EXPAND_SZ));

            data->swap(value);
            return S_OK;
        }
        CATCH_RETURN()

        inline HRESULT get_value_expanded_wstring_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_ ::std::wstring* data) WI_NOEXCEPT
        {
            return get_value_expanded_wstring_nothrow(key, nullptr, value_name, data);
        }
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)


        //
        // get_value_multistring, get_value_multistring_nothrow, try_get_value_multistring
        //
#if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        inline ::std::vector<::std::wstring> get_value_multistring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            ::std::vector<::std::wstring> return_value;
            ::std::vector<BYTE> rawData{ get_value_byte_vector(key, subkey, value_name, REG_MULTI_SZ) };
            if (!rawData.empty())
            {
                auto* const begin = reinterpret_cast<wchar_t*>(rawData.data());
                auto* const end = begin + rawData.size() / sizeof(wchar_t);
                return_value = ::wil::reg::details::get_wstring_vector_from_multistring(::std::vector<wchar_t>(begin, end));
            }

            return return_value;
        }

        inline ::std::vector<::std::wstring> get_value_multistring(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return get_value_multistring(key, nullptr, value_name);
        }

        inline HRESULT get_value_multistring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT try
        {
            return_value->clear();

            ::std::vector<BYTE> rawData;
            RETURN_IF_FAILED(::wil::reg::get_value_byte_vector_nothrow(key, subkey, value_name, REG_MULTI_SZ, &rawData));

            if (!rawData.empty())
            {
                auto* const begin = reinterpret_cast<wchar_t*>(rawData.data());
                auto* const end = begin + rawData.size() / sizeof(wchar_t);
                *return_value = ::wil::reg::details::get_wstring_vector_from_multistring(::std::vector<wchar_t>(begin, end));
            }
            return S_OK;
        }
        CATCH_RETURN()

            inline HRESULT get_value_multistring_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_multistring_nothrow(key, nullptr, value_name, return_value);
        }

#if defined (_OPTIONAL_) && defined(__cpp_lib_optional)
        inline ::std::optional<::std::vector<::std::wstring>> try_get_value_multistring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            ::std::vector<::std::wstring> value;
            const auto hr = ::wil::reg::get_value_multistring_nothrow(key, subkey, value_name, &value);
            if (SUCCEEDED(hr))
            {
                return { value };
            }

            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                return { ::std::nullopt };
            }

            THROW_HR(HRESULT_FROM_WIN32(hr));
        }

        inline ::std::optional<::std::vector<::std::wstring>> try_get_value_multistring(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_multistring(key, nullptr, value_name);
        }
#endif // #if defined (_OPTIONAL_) && defined(__cpp_lib_optional)

        template<>
        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_multistring_nothrow(key, nullptr, value_name, return_value);
        }
        template<>
        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_multistring_nothrow(key, subkey, value_name, return_value);
        }
#endif // #if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

} // namespace reg
} // namespace wil
#endif // __WIL_REGISTRY_BASIC_INCLUDED