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
// ReSharper disable CppInconsistentNaming
#ifndef __WIL_REGISTRY_BASIC_INCLUDED
#define __WIL_REGISTRY_BASIC_INCLUDED

// TODO: Yes - we should use feature checks instead of forced imports
//       We do *not* want to require all users to use the STL
//       We use the __WIL_WINREG_STL to track if we should support the STL (defined in resource.h)
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <list>

#include <Windows.h>
// include sddl.h if wanting to pass 
#include <sddl.h>

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
    constexpr bool is_value_was_not_found(HRESULT hr) WI_NOEXCEPT
    {
        return hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    constexpr bool is_value_was_too_large(HRESULT hr) WI_NOEXCEPT
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
        constexpr uint32_t iterator_end_offset = 0xffffffff;
        constexpr size_t iterator_default_buffer_size = 16;

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
            default:
                FAIL_FAST();
            }
        }

#if defined(__WIL_WINREG_STL)
        template <class InputIt>
        ::std::vector<wchar_t> get_multistring_from_wstrings(InputIt first, InputIt last)
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
                    // null terminator (1 of 2) or separator
                    multistring.push_back(L'\0');
                }
            }
            // null terminator (2 of 2)
            multistring.push_back(L'\0');
            return multistring;
        }

        inline ::std::vector<::std::wstring> get_wstring_vector_from_multistring(const ::std::vector<wchar_t>& data)
        {
            ::std::vector<::std::wstring> strings;

            if (data.empty())
            {
                return strings;
            }

            auto current = data.begin();
            while (current != data.end())
            {
                const auto next = ::std::find(current, data.end(), L'\0');
                ::std::wstring tempString(current, next);
                if (!tempString.empty())
                {
                    strings.emplace_back(::std::move(tempString));
                }
                if (next == data.end())
                {
                    break;
                }
                current = next + 1;
            }
            return strings;
        }

        enum class iterator_creation_flag
        {
            begin,
            end
        };

        struct key_iterator_data
        {
            // *not* owning the raw HKEY resource
            HKEY m_hkey{};
            ::std::vector<wchar_t> m_nextName;
            uint32_t m_index = iterator_end_offset;
        };

        struct value_iterator_data
        {
            // *not* owning the raw HKEY resource
            HKEY m_hkey{};
            DWORD m_nextType = REG_NONE;
            ::std::vector<wchar_t> m_nextName;
            uint32_t m_index = iterator_end_offset;
        };
#endif // #if defined(__WIL_WINREG_STL)

#ifdef __SDDL_H__
        template <typename err_policy>
        ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> create_security_descriptor(_In_opt_ PCWSTR security_descriptor)
        {
            ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> securityDescriptor;
            if (security_descriptor)
            {
                const auto convert_succeeded = ::ConvertStringSecurityDescriptorToSecurityDescriptorW(
                    security_descriptor,
                    SDDL_REVISION_1,
                    ::wil::out_param_ptr<PSECURITY_DESCRIPTOR*>(securityDescriptor),
                    nullptr);
                err_policy::LastErrorIfFalse(convert_succeeded);
            }
            return securityDescriptor;
        }
#endif

        constexpr HKEY get_key(HKEY h) WI_NOEXCEPT
        {
            return h;
        }

        inline HKEY get_key(const ::wil::unique_hkey& h) WI_NOEXCEPT
        {
            return h.get();
        }

#if defined(__WIL_WINREG_STL)
        inline HKEY get_key(const ::wil::shared_hkey& h) WI_NOEXCEPT
        {
            return h.get();
        }
#endif // #if defined(__WIL_WINREG_STL)

        // should_return_not_found needs a well-known error policy for T.
        template <typename T>
        constexpr bool should_return_not_found();

        template <>
        constexpr bool should_return_not_found<::wil::err_returncode_policy>()
        {
            return true;
        }

        template <>
        constexpr bool should_return_not_found<::wil::err_exception_policy>()
        {
            return false;
        }

        template <>
        constexpr bool should_return_not_found<::wil::err_failfast_policy>()
        {
            return false;
        }

#if defined(__WIL_WINREG_STL)
        // These iterator classes facilitates STL iterator semantics to walk a set of subkeys and values
        // within an instantiated Key object. This can be directly constructed passing a unique_hkey
        // but will likely be more commonly constructed from reg_view_t::reg_enum_keys() and reg_view_t::reg_enum_values()
        template <typename T>
        class iterator_t;

        // The template implementation for registry iterator support 2 types - for keys and values
        // These typedefs are designed to simplify types for callers and mask the implementation details
        using key_iterator = iterator_t<::wil::reg::details::key_iterator_data>;
        using value_iterator = iterator_t<::wil::reg::details::value_iterator_data>;

        template <typename T>
        class iterator_t
        {
        public:
            // defining iterator_traits allows STL <algorithm> functions to be used with this iterator class.
            // Notice this is a forward_iterator
            // - does not support random-access (e.g. vector::iterator)
            // - does not support bi-directional access (e.g. list::iterator)
            using iterator_category = ::std::forward_iterator_tag;
            using value_type = PCWSTR;
            using difference_type = size_t;
            using distance_type = size_t;
            using pointer = PCWSTR*;
            using reference = PCWSTR;

            iterator_t() = default;
            ~iterator_t() = default;

            iterator_t(HKEY hkey, ::wil::reg::details::iterator_creation_flag type)
            {
                m_data.m_hkey = hkey;
                m_data.m_nextName.resize(type == ::wil::reg::details::iterator_creation_flag::begin ? ::wil::reg::details::iterator_default_buffer_size : 0);
                m_data.m_index = type == ::wil::reg::details::iterator_creation_flag::begin ? 0 : ::wil::reg::details::iterator_end_offset;

                if (type == ::wil::reg::details::iterator_creation_flag::begin)
                {
                    this->enumerate_next();
                }
            }

            // copy construction can throw ::std::bad_alloc
            iterator_t(const iterator_t&) = default;
            iterator_t& operator=(const iterator_t&) = default;

            // moves cannot fail/throw
            iterator_t(iterator_t&& rhs) WI_NOEXCEPT = default;
            iterator_t& operator=(iterator_t&& rhs) WI_NOEXCEPT = default;

            // operator support
            PCWSTR operator*() const
            {
                if (::wil::reg::details::iterator_end_offset == m_data.m_index)
                {
                    THROW_WIN32(ERROR_NO_MORE_ITEMS);
                }
                return m_data.m_nextName.size() < 2 ? L"" : &m_data.m_nextName[0];
            }

            bool operator==(const iterator_t& rhs) const WI_NOEXCEPT
            {
                if (::wil::reg::details::iterator_end_offset == m_data.m_index || ::wil::reg::details::iterator_end_offset == rhs.m_data.m_index)
                {
                    // if either is not initialized (or end), both must not be initialized (or end) to be equal
                    return m_data.m_index == rhs.m_data.m_index;
                }
                return m_data.m_hkey == rhs.m_data.m_hkey && m_data.m_index == rhs.m_data.m_index;
            }

            bool operator!=(const iterator_t& rhs) const WI_NOEXCEPT
            {
                return !(*this == rhs);
            }

            // pre-increment
            iterator_t& operator++()
            {
                this->operator +=(1);
                return *this;
            }

            // increment by integer
            iterator_t& operator+=(size_t offset)
            {
                // fail on overflow
                uint32_t newIndex = m_data.m_index + static_cast<uint32_t>(offset);
                if (newIndex < m_data.m_index)
                {
                    THROW_HR(E_INVALIDARG);
                }
                // fail if this creates an end iterator
                if (newIndex == ::wil::reg::details::iterator_end_offset)
                {
                    THROW_HR(E_INVALIDARG);
                }

                m_data.m_index = newIndex;
                this->enumerate_next();
                return *this;
            }

            // not supporting post-increment - which would require copy-construction
            iterator_t operator++(int) = delete;

            // access type information directly off the iterator
            [[nodiscard]] DWORD type() const WI_NOEXCEPT;

        private:
            void make_end_iterator()
            {
                T temp;
                m_data = temp;
            }

            // must be specialized per template type
            void enumerate_next();

            // container based on the class template type
            T m_data{};
        };

        template <>
        inline void iterator_t<::wil::reg::details::key_iterator_data>::enumerate_next()
        {
            for (auto vectorSize = static_cast<DWORD>(m_data.m_nextName.capacity());;)
            {
                m_data.m_nextName.resize(vectorSize);
                auto tempVectorSize = vectorSize;
                const auto error = ::RegEnumKeyExW(
                    m_data.m_hkey,
                    m_data.m_index,
                    m_data.m_nextName.data(),
                    &tempVectorSize,
                    nullptr, // reserved
                    nullptr, // not concerned about class name
                    nullptr, // not concerned about the size of the class name
                    nullptr); // not concerned about the last write time

                if (error == ERROR_SUCCESS)
                {
                    break;
                }
                if (error == ERROR_NO_MORE_ITEMS)
                {
                    // set the end() iterator
                    this->make_end_iterator();
                    break;
                }
                if (error == ERROR_MORE_DATA)
                {
                    // resize to one-more-than-returned for the null-terminator
                    // can continue the loop
                    vectorSize = tempVectorSize + 1;
                    continue;
                }

                // any other error will throw
                THROW_WIN32(error);
            }
        }

        template <>
        inline void iterator_t<::wil::reg::details::value_iterator_data>::enumerate_next()
        {
            for (auto vectorSize = static_cast<DWORD>(m_data.m_nextName.capacity());;)
            {
                m_data.m_nextName.resize(vectorSize);
                auto tempVectorSize = vectorSize;
                const auto error = ::RegEnumValueW(
                    m_data.m_hkey,
                    m_data.m_index,
                    m_data.m_nextName.data(),
                    &tempVectorSize,
                    nullptr, // reserved
                    &m_data.m_nextType,
                    nullptr, // not concerned about the data in the value
                    nullptr); // not concerned about the data in the value
                if (error == ERROR_SUCCESS)
                {
                    break;
                }

                if (error == ERROR_NO_MORE_ITEMS)
                {
                    // set the end() iterator
                    this->make_end_iterator();
                    break;
                }

                if (error == ERROR_MORE_DATA)
                {
                    // resize to one-more-than-returned for the null-terminator
                    // can continue the loop
                    vectorSize = tempVectorSize + 1;
                    continue;
                }

                // any other error will throw
                THROW_WIN32(error);
            }
        }

        // ReSharper disable once CppMemberFunctionMayBeStatic
        template <>
        inline DWORD iterator_t<::wil::reg::details::key_iterator_data>::type() const WI_NOEXCEPT
        {
            return REG_NONE;
        }

        template <>
        inline DWORD iterator_t<::wil::reg::details::value_iterator_data>::type() const WI_NOEXCEPT
        {
            return m_data.m_nextType;
        }
#endif // #if defined(__WIL_WINREG_STL)
    }

    template <typename T>
    struct optional_value
    {
        constexpr bool has_value() const WI_NOEXCEPT
        {
            return value_assigned;
        }
        constexpr explicit operator bool() const WI_NOEXCEPT
        {
            return value_assigned;
        }

        [[nodiscard]] constexpr const T& value_or(const T& left_value) const
        {
            return value_assigned ? value : left_value;
        }

        T value{};
        bool value_assigned{ false };
        HRESULT inner_error{ E_NOT_VALID_STATE };
    };

    namespace reg_view_details
    {
        // generic functions that can apply to both specialized and non-specialized types
        // that are assigned into ::wil::reg::optional_value types

        // TODO: do we need a default version, or should we fail for unknown types?
        /*template <typename T>
        constexpr void* get_buffer(const T& t) WI_NOEXCEPT
        {
            return const_cast<T*>(&t);
        }*/

#if defined(__WIL_WINREG_STL)
        inline void* get_buffer(const ::std::vector<BYTE>& buffer) WI_NOEXCEPT
        {
            return const_cast<BYTE*>(buffer.data());
        }

        inline void* get_buffer(const ::std::wstring& string) WI_NOEXCEPT
        {
            return const_cast<wchar_t*>(string.data());
        }
#endif // #if defined(__WIL_WINREG_STL)

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
        inline void* get_buffer(const ::wil::unique_bstr& value) WI_NOEXCEPT
        {
            return value.get();
        }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OLEAUTO_H_STL)
        inline void* get_buffer(const ::wil::shared_bstr& value) WI_NOEXCEPT
        {
            return value.get();
        }
#endif // #if defined(__WIL_OLEAUTO_H_STL)

        // TODO: do we need a generic version or should we have a compile error?
        /*template <typename T>
        constexpr DWORD get_buffer_size(const T) WI_NOEXCEPT
        {
            return sizeof(T);
        }*/

#if defined(__WIL_WINREG_STL)
        inline DWORD get_buffer_size(const ::std::vector<BYTE>& buffer) WI_NOEXCEPT
        {
            return static_cast<DWORD>(buffer.size());
        }

        inline DWORD get_buffer_size(const ::std::wstring& string) WI_NOEXCEPT
        {
            return static_cast<DWORD>(string.size() * sizeof(wchar_t));
        }
#endif // #if defined(__WIL_WINREG_STL)

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
        inline DWORD get_buffer_size(const ::wil::unique_bstr& value) WI_NOEXCEPT
        {
            return ::SysStringLen(value.get());
        }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OLEAUTO_H_STL)
        inline DWORD get_buffer_size(const ::wil::shared_bstr& value) WI_NOEXCEPT
        {
            return ::SysStringLen(value.get());
        }
#endif // #if defined(__WIL_OLEAUTO_H_STL)

        // grow_buffer_if_supported returns true if growing that buffer type is supported, false if growing that type is not supported
        // the caller will call get_buffer_size later to validate if the allocation succeeded

        template <typename T>
        constexpr bool grow_buffer_if_supported(T&, DWORD) WI_NOEXCEPT
        {
            return false;
        }

#if defined(__WIL_WINREG_STL)
        inline bool grow_buffer_if_supported(::std::vector<BYTE>& buffer, DWORD byteSize) WI_NOEXCEPT
        {
            try
            {
                // reset to size 0 in case resize() throws
                buffer.clear();
                buffer.resize(byteSize);
            }
            catch (...)
            {
            }
            return true;
        }

        inline bool grow_buffer_if_supported(::std::wstring& string, DWORD byteSize) WI_NOEXCEPT
        {
            try
            {
                // reset to size 0 in case resize() throws
                string.clear();
                string.resize(byteSize / sizeof(wchar_t));
            }
            catch (...)
            {
            }
            return true;
        }
#endif // #if defined(__WIL_WINREG_STL)

#if defined(__WIL_OLEAUTO_H_)
        inline bool grow_buffer_if_supported(::wil::unique_bstr& string, DWORD byteSize) WI_NOEXCEPT
        {
            string.reset(::SysAllocStringByteLen(nullptr, byteSize));
            if (string)
            {
                ZeroMemory(string.get(), byteSize);
            }
            return true;
        }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OLEAUTO_H_STL)
        inline bool grow_buffer_if_supported(::wil::shared_bstr& string, DWORD byteSize) WI_NOEXCEPT
        {
            string.reset(::SysAllocStringByteLen(nullptr, byteSize));
            if (string)
            {
                ZeroMemory(string.get(), byteSize);
            }
            return true;
        }
#endif // #if defined(__WIL_OLEAUTO_H_STL)

        template <typename T>
        constexpr void trim_buffer(T&) WI_NOEXCEPT
        {
        }

#if defined(__WIL_WINREG_STL)
        inline void trim_buffer(::std::wstring& buffer)
        {
            const auto offset = buffer.find_first_of(L'\0');
            if (offset != ::std::wstring::npos)
            {
                buffer.resize(offset);
            }
        }
#endif // #if defined(__WIL_WINREG_STL)

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

#if defined(__WIL_WINREG_STL)
        template <>
        constexpr DWORD get_value_type<::std::wstring>() WI_NOEXCEPT
        {
            return ::wil::reg::details::get_value_flags_from_value_type(REG_SZ);
        }
#endif // #if defined(__WIL_WINREG_STL)

#if defined(__WIL_OLEAUTO_H_)
        template <>
        constexpr DWORD get_value_type<wil::unique_bstr>() WI_NOEXCEPT
        {
            return ::wil::reg::details::get_value_flags_from_value_type(REG_SZ);
        }
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OLEAUTO_H_STL)
        template <>
        constexpr DWORD get_value_type<wil::shared_bstr>() WI_NOEXCEPT
        {
            return ::wil::reg::details::get_value_flags_from_value_type(REG_SZ);
        }
#endif // #if defined(__WIL_OLEAUTO_H_STL)

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

#if defined(__WIL_WINREG_STL)
        constexpr DWORD set_value_type(const ::std::wstring&) WI_NOEXCEPT
        {
            return REG_SZ;
        }
#endif // #if defined(__WIL_WINREG_STL)

        constexpr DWORD set_value_type(PCWSTR) WI_NOEXCEPT
        {
            return REG_SZ;
        }

        // type T is the owning type for storing a key: HKEY, ::wil::unique_hkey, ::wil::shared_hkey
        template <typename T, typename err_policy = ::wil::err_exception_policy>
        class reg_view_t
        {
        public:
            template <typename K>
            explicit reg_view_t(K&& key) WI_NOEXCEPT :
                m_key(::wistd::forward<K>(key))
            {
            }

            [[nodiscard]] HKEY get_key() const WI_NOEXCEPT
            {
                return ::wil::reg::details::get_key(m_key);
            }

            typename err_policy::result open_key(_In_opt_ PCWSTR subKey, ::wil::unique_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
            {
                auto error = ::RegOpenKeyExW(::wil::reg::details::get_key(m_key), subKey, 0, ::wil::reg::details::get_access_flags(access), ::wil::out_param(hkey));
                if (error == ERROR_FILE_NOT_FOUND && !::wil::reg::details::should_return_not_found<err_policy>())
                {
                    error = ERROR_SUCCESS;
                    hkey.reset();
                }
                return err_policy::HResult(HRESULT_FROM_WIN32(error));
            }

            ::wil::unique_hkey open_unique_key(_In_opt_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
            {
                ::wil::unique_hkey local_key{};
                open_key(subKey, local_key, access);
                return local_key;
            }

#if defined(__WIL_WINREG_STL)
            typename err_policy::result open_key(_In_opt_ PCWSTR subKey, ::wil::shared_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
            {
                auto error = ::RegOpenKeyExW(::wil::reg::details::get_key(m_key), subKey, 0, ::wil::reg::details::get_access_flags(access), ::wil::out_param(hkey));
                if (error == ERROR_FILE_NOT_FOUND && !::wil::reg::details::should_return_not_found<err_policy>())
                {
                    error = ERROR_SUCCESS;
                    hkey.reset();
                }
                return err_policy::HResult(HRESULT_FROM_WIN32(error));
            }
            ::wil::shared_hkey open_shared_key(_In_opt_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
            {
                ::wil::shared_hkey local_key{};
                open_key(subKey, local_key, access);
                return local_key;
            }
#endif // #if defined(__WIL_WINREG_STL)

#ifdef __SDDL_H__
            typename err_policy::result create_key(_In_ PCWSTR subKey, ::wil::unique_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr) const
            {
                hkey.reset();

                const ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> securityDescriptor = ::wil::reg::details::create_security_descriptor<err_policy>(security_descriptor);
                SECURITY_ATTRIBUTES security_attributes{ sizeof security_attributes, nullptr, FALSE };
                security_attributes.lpSecurityDescriptor = securityDescriptor.get();

                DWORD disposition = 0;
                const auto error =
                    ::RegCreateKeyExW(::wil::reg::details::get_key(m_key), subKey, 0, nullptr, 0, ::wil::reg::details::get_access_flags(access), security_descriptor ? &security_attributes : nullptr, ::wil::out_param(hkey), &disposition);
                return err_policy::HResult(HRESULT_FROM_WIN32(error));
            }

            ::wil::unique_hkey create_unique_key(_In_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr) const
            {
                ::wil::unique_hkey local_key{};
                create_key(subKey, local_key, access, security_descriptor);
                return local_key;
            }
#else
            typename err_policy::result create_key(_In_ PCWSTR subKey, ::wil::unique_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
            {
                hkey.reset();
                DWORD disposition = 0;
                const auto error =
                    ::RegCreateKeyExW(::wil::reg::details::get_key(m_key), subKey, 0, nullptr, 0, ::wil::reg::details::get_access_flags(access), nullptr, ::wil::out_param(hkey), &disposition);
                return err_policy::HResult(HRESULT_FROM_WIN32(error));
            }

            ::wil::unique_hkey create_unique_key(_In_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
            {
                ::wil::unique_hkey local_key{};
                create_key(subKey, local_key, access);
                return local_key;
            }
#endif // #ifdef __SDDL_H__


#if defined(__WIL_WINREG_STL)
#ifdef __SDDL_H__
            typename err_policy::result create_key(_In_ PCWSTR subKey, ::wil::shared_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr) const
            {
                hkey.reset();

                const ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> securityDescriptor = ::wil::reg::details::create_security_descriptor<err_policy>(security_descriptor);

                SECURITY_ATTRIBUTES security_attributes{ sizeof security_attributes, nullptr, FALSE };
                security_attributes.lpSecurityDescriptor = securityDescriptor.get();

                DWORD disposition = 0;
                const auto error =
                    ::RegCreateKeyExW(::wil::reg::details::get_key(m_key), subKey, 0, nullptr, 0, ::wil::reg::details::get_access_flags(access), security_descriptor ? &security_attributes : nullptr, ::wil::out_param(hkey), &disposition);
                return err_policy::HResult(HRESULT_FROM_WIN32(error));
            }

            ::wil::shared_hkey create_shared_key(_In_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr) const
            {
                ::wil::shared_hkey local_key{};
                create_key(subKey, local_key, access, security_descriptor);
                return local_key;
            }
#else
            typename err_policy::result create_key(_In_ PCWSTR subKey, ::wil::shared_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
            {
                hkey.reset();

                DWORD disposition = 0;
                const auto error =
                    ::RegCreateKeyExW(::wil::reg::details::get_key(m_key), subKey, 0, nullptr, 0, ::wil::reg::details::get_access_flags(access), nullptr, ::wil::out_param(hkey), &disposition);
                return err_policy::HResult(HRESULT_FROM_WIN32(error));
            }

            ::wil::shared_hkey create_shared_key(_In_ PCWSTR subKey, ::wil::reg::key_access access = ::wil::reg::key_access::read) const
            {
                ::wil::shared_hkey local_key{};
                create_key(subKey, local_key, access);
                return local_key;
            }
#endif // #ifdef __SDDL_H__

#endif // #if defined(__WIL_WINREG_STL)

            typename err_policy::result delete_key(_In_opt_ PCWSTR sub_key) WI_NOEXCEPT
            {
                auto error = RegDeleteTreeW(::wil::reg::details::get_key(m_key), sub_key);
                if (error == ERROR_FILE_NOT_FOUND)
                {
                    error = ERROR_SUCCESS;
                }
                return err_policy::HResult(HRESULT_FROM_WIN32(error));
            }

            typename err_policy::result delete_value(_In_opt_ PCWSTR value_name) WI_NOEXCEPT
            {
                return err_policy::HResult(HRESULT_FROM_WIN32(RegDeleteValueW(::wil::reg::details::get_key(m_key), value_name)));
            }

        private:

            template <typename R, typename get_value_internal_policy = err_policy>
            typename get_value_internal_policy::result get_value_internal(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, R& return_value, DWORD type = ::wil::reg::reg_view_details::get_value_type<R>()) const
            {
                for (;;)
                {
                    DWORD data_size_bytes{ ::wil::reg::reg_view_details::get_buffer_size(return_value) };
                    auto error = ::RegGetValueW(
                        ::wil::reg::details::get_key(m_key),
                        subkey,
                        value_name,
                        ::wil::reg::details::get_value_flags_from_value_type(type),
                        nullptr,
                        ::wil::reg::reg_view_details::get_buffer(return_value),
                        &data_size_bytes);
                    // GetRegValueW will indicate the caller allocate the returned number of bytes in one of two cases:
                    // 1. returns ERROR_MORE_DATA
                    // 2. returns ERROR_SUCCESS when we gave it a nullptr for the out buffer
                    const bool shouldReallocate = (error == ERROR_MORE_DATA) || (error == ERROR_SUCCESS && ::wil::reg::reg_view_details::get_buffer(return_value) == nullptr);
                    if (shouldReallocate && ::wil::reg::reg_view_details::grow_buffer_if_supported(return_value, data_size_bytes))
                    {
                        // verify if grow_buffer_if_supported succeeded allocation
                        if (::wil::reg::reg_view_details::get_buffer_size(return_value) == 0)
                        {
                            // will return this error after the if block
                            error = ERROR_NOT_ENOUGH_MEMORY;
                        }
                        else
                        {
                            // if it succeeds, continue the for loop to try again
                            continue;
                        }
                    }

                    if (error == ERROR_SUCCESS)
                    {
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
                return get_value(nullptr, value_name, return_value, type);
            }

            // intended for err_exception_policy as err_returncode_policy will not get an error code
            template <typename R>
            ::wil::reg::optional_value<R> try_get_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type = ::wil::reg::reg_view_details::get_value_type<R>()) const
            {
                ::wil::reg::optional_value<R> return_value;

                return_value.inner_error = get_value_internal<R, ::wil::err_returncode_policy>(subkey, value_name, return_value.value, type);
                if (SUCCEEDED(return_value.inner_error))
                {
                    return_value.value_assigned = true;
                    return return_value;
                }

                if (return_value.inner_error == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                {
                    return_value.value = {};
                    return return_value;
                }

                // throw if exception policy
                err_policy::HResult(HRESULT_FROM_WIN32(return_value.inner_error));
                return return_value;
            }

            template <typename R>
            ::wil::reg::optional_value<R>& try_get_value(_In_opt_ PCWSTR value_name, DWORD type = ::wil::reg::reg_view_details::get_value_type<R>()) const
            {
                return try_get_value(nullptr, value_name, type);
            }

            template <typename R>
            typename err_policy::result set_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const R& value) const
            {
                const auto error = ::RegSetKeyValueW(
                    ::wil::reg::details::get_key(m_key),
                    subkey,
                    value_name,
                    ::wil::reg::reg_view_details::set_value_type(value),
                    static_cast<BYTE*>(::wil::reg::reg_view_details::get_buffer(value)),
                    ::wil::reg::reg_view_details::get_buffer_size(value));
                return err_policy::HResult(HRESULT_FROM_WIN32(error));
            }

            template <typename R>
            typename err_policy::result set_value(_In_opt_ PCWSTR value_name, const R& value) const
            {
                return set_value(nullptr, value_name, value);
            }

#if defined(__WIL_WINREG_STL)
            template <typename R>
            typename err_policy::result set_value_multistring(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const R& data) const try
            {
                const auto multiStringWcharVector(::wil::reg::details::get_multistring_from_wstrings(::std::begin(data), ::std::end(data)));

                const auto* byteArray = reinterpret_cast<const BYTE*>(&multiStringWcharVector[0]);
                const auto byteArrayLength = static_cast<DWORD>(multiStringWcharVector.size() * sizeof(wchar_t));
                const auto error = ::RegSetKeyValueW(
                    ::wil::reg::details::get_key(m_key),
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

            struct key_enumerator
            {
                explicit key_enumerator(HKEY key) :
                    m_hkey(key)
                {
                }

                [[nodiscard]] ::wil::reg::details::key_iterator begin() const
                {
                    return ::wil::reg::details::key_iterator(m_hkey, ::wil::reg::details::iterator_creation_flag::begin);
                }

                [[nodiscard]] ::wil::reg::details::key_iterator end() const WI_NOEXCEPT
                {
                    return ::wil::reg::details::key_iterator(m_hkey, ::wil::reg::details::iterator_creation_flag::end);
                }

            private:
                friend class reg_view_t;
                // non-owning resource (just get a copy from the owner)
                HKEY m_hkey;
            };

            struct value_enumerator
            {
                explicit value_enumerator(HKEY key) :
                    m_hkey(key)
                {
                }

                // begin() will throw Registry::Exception on error 
                [[nodiscard]] ::wil::reg::details::value_iterator begin() const
                {
                    return ::wil::reg::details::value_iterator(m_hkey, ::wil::reg::details::iterator_creation_flag::begin);
                }

                [[nodiscard]] ::wil::reg::details::value_iterator end() const WI_NOEXCEPT
                {
                    return ::wil::reg::details::value_iterator(m_hkey, ::wil::reg::details::iterator_creation_flag::end);
                }

            private:
                friend class reg_view_t;
                // non-owning resource (just get a copy from the owner)
                HKEY m_hkey;
            };

            [[nodiscard]] key_enumerator reg_enum_keys() const
            {
                return key_enumerator(::wil::reg::details::get_key(m_key));
            }

            [[nodiscard]] value_enumerator reg_enum_values() const
            {
                return value_enumerator(::wil::reg::details::get_key(m_key));
            }
#endif // #if defined(__WIL_WINREG_STL

        private:
            T m_key{};
        };

        template <typename T, typename err_policy = err_exception_policy>
        static reg_view_t<T, err_policy> create_reg_view(T&& t)
        {
            return reg_view_t<T, err_policy>(::wistd::forward<T>(t));
        }

        // reg_view with the raw HKEY type is non-owning
        using reg_view_nothrow = reg_view_details::reg_view_t<HKEY, ::wil::err_returncode_policy>;
        using reg_view_unique_hkey_nothrow = reg_view_details::reg_view_t<::wil::unique_hkey, ::wil::err_returncode_policy>;
#if defined(__WIL_WINREG_STL)
        using reg_view_shared_hkey_nothrow = reg_view_details::reg_view_t<::wil::shared_hkey, ::wil::err_returncode_policy>;
#endif // #if defined(__WIL_WINREG_STL)

#if defined(WIL_ENABLE_EXCEPTIONS)
        // reg_view with the raw HKEY type is non-owning
        using reg_view = reg_view_details::reg_view_t<HKEY, ::wil::err_exception_policy>;
        using reg_view_unique_hkey = reg_view_details::reg_view_t<::wil::unique_hkey, ::wil::err_exception_policy>;
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)
#if defined(__WIL_WINREG_STL)
        using reg_view_shared_hkey = reg_view_details::reg_view_t<::wil::shared_hkey, ::wil::err_exception_policy>;
#endif // #if defined(__WIL_WINREG_STL)
    }

    // Registry Open* and Create* functions, both throwing and non-throwing
    // 
    // If wanting to return a wil::unique_key:
    //   open_unique_key, create_unique_key => throws on error
    //   try_open_unique_key, try_create_unique_key => returns null on error
    //
    // If wanting to return a wil::shared_key (requires including wil/stl.h):
    //   open_shared_key, create_shared_key => throws on error
    //   try_open_shared_key, try_create_shared_key => returns null on error
    //
    // If wanting an HRESULT return code:
    //   open_key_nothrow, create_key_nothrow

#if defined(WIL_ENABLE_EXCEPTIONS)
    inline ::wil::unique_hkey open_unique_key(HKEY key, _In_opt_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.open_unique_key(path, access);
    }

    inline ::wil::unique_hkey create_unique_key(HKEY key, _In_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.create_unique_key(path, access, security_descriptor);
    }
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

    inline ::wil::unique_hkey try_open_unique_key(HKEY key, _In_opt_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.open_unique_key(path, access);
    }

    inline ::wil::unique_hkey try_create_unique_key(HKEY key, _In_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.create_unique_key(path, access, security_descriptor);
    }

#if defined(__WIL_WINREG_STL)
    inline ::wil::shared_hkey open_shared_key(HKEY key, _In_opt_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.open_shared_key(path, access);
    }

    inline ::wil::shared_hkey create_shared_key(HKEY key, _In_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.create_shared_key(path, access, security_descriptor);
    }

    inline ::wil::shared_hkey try_open_shared_key(HKEY key, _In_opt_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.open_shared_key(path, access);
    }

    inline ::wil::shared_hkey try_create_shared_key(HKEY key, _In_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.create_shared_key(path, access, security_descriptor);
    }
#endif // #if defined(__WIL_WINREG_STL)

    inline HRESULT open_key_nothrow(HKEY key, _In_opt_ PCWSTR path, ::wil::unique_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.open_key(path, hkey, access);
    }

#if defined(__WIL_WINREG_STL)
    inline HRESULT open_key_nothrow(HKEY key, _In_opt_ PCWSTR path, ::wil::shared_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.open_key(path, hkey, access);
    }
#endif // #if defined(__WIL_WINREG_STL)

#ifdef __SDDL_H__
    inline HRESULT create_key_nothrow(HKEY key, _In_ PCWSTR path, ::wil::unique_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.create_key(path, hkey, access, security_descriptor);
    }
#else
    inline HRESULT create_key_nothrow(HKEY key, _In_ PCWSTR path, ::wil::unique_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read)
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.create_key(path, hkey, access);
    }
#endif // #ifdef __SDDL_H__

#if defined(__WIL_WINREG_STL)
    inline HRESULT create_key_nothrow(HKEY key, _In_ PCWSTR path, ::wil::shared_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.create_key(path, hkey, access, security_descriptor);
    }
#endif // #if defined(__WIL_WINREG_STL)

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

    // Registry Set* functions, both throwing and non-throwing
    //
    // If you want the call to set* to throw on error:
    //   You can call the templatized version if you don't want to specify the type in the function name:
    //     set_value
    //   Or you can specify the name in the function:
    //     set_value_dword, set_value_qword, set_value_string, set_value_multistring
    //
    // If you want to return an HRESULT instead of throw:
    //   You can call the template version if you don't want to specify the type in the function name:
    //     set_value_nothrow
    //   Or you can specify the name in the function:
    //     set_value_dword_nothrow, set_value_qword_nothrow, set_value_string_nothrow, set_value_multistring_nothrow


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

    inline void set_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD data)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.set_value(subkey, value_name, data);
    }

    inline void set_value_dword(HKEY key, _In_opt_ PCWSTR value_name, DWORD data)
    {
        return set_value_dword(key, nullptr, value_name, data);
    }

    inline void set_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint64_t data)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.set_value(subkey, value_name, data);
    }

    inline void set_value_qword(HKEY key, _In_opt_ PCWSTR value_name, uint64_t data)
    {
        return set_value_qword(key, nullptr, value_name, data);
    }

    inline void set_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ PCWSTR data)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.set_value(subkey, value_name, data);
    }

    inline void set_value_string(HKEY key, _In_opt_ PCWSTR value_name, _In_ PCWSTR data)
    {
        return set_value_string(key, nullptr, value_name, data);
    }
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_WINREG_STL)
    inline void set_value_multistring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.set_value_multistring(subkey, value_name, data);
    }

    inline void set_value_multistring(HKEY key, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
    {
        return set_value_multistring(key, nullptr, value_name, data);
    }

    inline void set_value_multistring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const ::std::list<::std::wstring>& data)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.set_value_multistring(subkey, value_name, data);
    }

    inline void set_value_multistring(HKEY key, _In_opt_ PCWSTR value_name, const ::std::list<::std::wstring>& data)
    {
        return set_value_multistring(key, nullptr, value_name, data);
    }
#endif // #if defined(__WIL_WINREG_STL)


    //
    // set_value_*_nothrow functions
    //
    template <typename T>
    HRESULT set_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, const T& data) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.set_value(value_name, data);
    }

    template <typename T>
    HRESULT set_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const T& data) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.set_value(subkey, value_name, data);
    }

    inline HRESULT set_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD data) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.set_value(subkey, value_name, data);
    }

    inline HRESULT set_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, DWORD data) WI_NOEXCEPT
    {
        return set_value_dword_nothrow(key, nullptr, value_name, data);
    }

    inline HRESULT set_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint64_t data) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.set_value(subkey, value_name, data);
    }

    inline HRESULT set_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, uint64_t data) WI_NOEXCEPT
    {
        return set_value_qword_nothrow(key, nullptr, value_name, data);
    }

    inline HRESULT set_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.set_value(subkey, value_name, data);
    }

    inline HRESULT set_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
    {
        return set_value_string_nothrow(key, nullptr, value_name, data);
    }

#if defined(__WIL_WINREG_STL)
    inline HRESULT set_value_multistring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_ PCWSTR value_name, const ::std::vector<::std::wstring>& data) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.set_value_multistring(subkey, value_name, data);
    }
    inline HRESULT set_value_multistring_nothrow(HKEY key, _In_ PCWSTR value_name, const ::std::vector<::std::wstring>& data) WI_NOEXCEPT
    {
        return set_value_multistring_nothrow(key, nullptr, value_name, data);
    }

    inline HRESULT set_value_multistring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_ PCWSTR value_name, const ::std::list<::std::wstring>& data) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.set_value_multistring(subkey, value_name, data);
    }
    inline HRESULT set_value_multistring_nothrow(HKEY key, _In_ PCWSTR value_name, const ::std::list<::std::wstring>& data) WI_NOEXCEPT
    {
        return set_value_multistring_nothrow(key, nullptr, value_name, data);
    }
#endif // #if defined(__WIL_WINREG_STL)


    //
    // get_value* and try_get_value* (throwing) functions
    //

#if defined(WIL_ENABLE_EXCEPTIONS)
/*
template <typename T>
T get_value(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const T& default_value)
{
// todo: assign default_value if doesn't exist (this won't work with things like strings. sigh)
UNREFERENCED_PARAMETER(default_value);
const reg_view_details::reg_view regview{ key };
T return_value{};
regview.get_value<T>(subkey, value_name, return_value);
return return_value;
}
*/

    inline DWORD get_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
    {
        const reg_view_details::reg_view regview{ key };
        DWORD return_value{};
        regview.get_value<DWORD>(subkey, value_name, return_value);
        return return_value;
    }

    inline DWORD get_value_dword(HKEY key, _In_opt_ PCWSTR value_name)
    {
        return get_value_dword(key, nullptr, value_name);
    }

    inline ::wil::reg::optional_value<DWORD> try_get_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.try_get_value<DWORD>(subkey, value_name);
    }

    inline ::wil::reg::optional_value<DWORD> try_get_value_dword(HKEY key, _In_opt_ PCWSTR value_name)
    {
        return try_get_value_dword(key, nullptr, value_name);
    }

    inline DWORD64 get_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
    {
        const reg_view_details::reg_view regview{ key };
        DWORD64 return_value{};
        regview.get_value<DWORD64>(subkey, value_name, return_value);
        return return_value;
    }

    inline DWORD64 get_value_qword(HKEY key, _In_opt_ PCWSTR value_name)
    {
        return get_value_qword(key, nullptr, value_name);
    }

    inline ::wil::reg::optional_value<DWORD64> try_get_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.try_get_value<DWORD64>(subkey, value_name);
    }

    inline ::wil::reg::optional_value<DWORD64> try_get_value_qword(HKEY key, _In_opt_ PCWSTR value_name)
    {
        return try_get_value_qword(key, nullptr, value_name);
    }
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_WINREG_STL)
    inline ::std::vector<BYTE> get_value_byte_vector(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type)
    {
        const reg_view_details::reg_view regview{ key };
        ::std::vector<BYTE> return_value{};
        regview.get_value<::std::vector<BYTE>>(subkey, value_name, return_value, type);
        return return_value;
    }

    inline ::std::vector<BYTE> get_value_byte_vector(HKEY key, _In_opt_ PCWSTR value_name, DWORD type)
    {
        return get_value_byte_vector(key, nullptr, value_name, type);
    }

    inline ::wil::reg::optional_value<::std::vector<BYTE>> try_get_value_byte_vector(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.try_get_value<::std::vector<BYTE>>(subkey, value_name, type);
    }

    inline ::wil::reg::optional_value<::std::vector<BYTE>> try_get_value_byte_vector(HKEY key, _In_opt_ PCWSTR value_name, DWORD type)
    {
        return try_get_value_byte_vector(key, nullptr, value_name, type);
    }

    inline ::std::wstring get_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
    {
        const reg_view_details::reg_view regview{ key };
        ::std::wstring return_value{};
        regview.get_value<::std::wstring>(subkey, value_name, return_value);
        return return_value;
    }

    inline ::std::wstring get_value_string(HKEY key, _In_opt_ PCWSTR value_name)
    {
        return get_value_string(key, nullptr, value_name);
    }

    inline ::wil::reg::optional_value<::std::wstring> try_get_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.try_get_value<::std::wstring>(subkey, value_name);
    }

    inline ::wil::reg::optional_value<::std::wstring> try_get_value_string(HKEY key, _In_opt_ PCWSTR value_name)
    {
        return try_get_value_string(key, nullptr, value_name);
    }

    inline ::std::wstring get_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
    {
        ::std::wstring value;
        const reg_view_details::reg_view regview{ key };
        regview.get_value<::std::wstring>(subkey, value_name, value, REG_EXPAND_SZ);
        return value;
    }

    inline ::std::wstring get_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name)
    {
        return get_value_expanded_string(key, nullptr, value_name);
    }

    inline ::wil::reg::optional_value<::std::wstring> try_get_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
    {
        const reg_view_details::reg_view regview{ key };
        return regview.try_get_value<::std::wstring>(subkey, value_name, REG_EXPAND_SZ);
    }

    inline ::wil::reg::optional_value<::std::wstring> try_get_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name)
    {
        return try_get_value_expanded_string(key, nullptr, value_name);
    }
#endif // #if defined(__WIL_WINREG_STL)

    template <size_t Length>
    HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* pRequiredBytes = nullptr) WI_NOEXCEPT
    {
        // don't allocate, just use their buffer
        wil::assign_to_opt_param(pRequiredBytes, 0ul);
        DWORD data_size_bytes{ Length * sizeof(WCHAR) };
        const auto error = RegGetValueW(key, subkey, value_name, ::wil::reg::details::get_value_flags_from_value_type(REG_SZ), nullptr, return_value, &data_size_bytes);
        if (error == ERROR_SUCCESS || error == ERROR_MORE_DATA)
        {
            wil::assign_to_opt_param(pRequiredBytes, data_size_bytes);
        }
        return HRESULT_FROM_WIN32(error);
    }

    template <size_t Length>
    HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* pRequiredBytes = nullptr) WI_NOEXCEPT
    {
        return get_value_string_nothrow<Length>(key, nullptr, value_name, return_value, pRequiredBytes);
    }

    // enable_if allows the correct get_value_nothrow when passed a WCHAR[] array
    // e.g.
    // WCHAR data[100]{};
    // get_value(hkye, L"valuename", data);

    template<typename T,
        typename std::enable_if<!std::is_same<T, wchar_t>::value>::type* = nullptr>
    HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_ T* return_value) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.get_value<T>(value_name, *return_value);
    }

    template<typename T,
        typename std::enable_if<!std::is_same<T, wchar_t>::value>::type* = nullptr>
    HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ T* return_value) WI_NOEXCEPT
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        return regview.get_value<T>(subkey, value_name, *return_value);
    }

    template <size_t Length>
    HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length]) WI_NOEXCEPT
    {
        return get_value_string_nothrow<Length>(key, subkey, value_name, return_value, nullptr);
    }

    template <size_t Length>
    HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length]) WI_NOEXCEPT
    {
        return get_value_string_nothrow<Length>(key, nullptr, value_name, return_value, nullptr);
    }

    inline HRESULT get_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ DWORD* return_value) WI_NOEXCEPT
    {
        DWORD data_size_bytes{ sizeof * return_value };
        return HRESULT_FROM_WIN32(RegGetValueW(key, subkey, value_name, ::wil::reg::details::get_value_flags_from_value_type(REG_DWORD), nullptr, return_value, &data_size_bytes));
    }

    inline HRESULT get_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_ DWORD* return_value) WI_NOEXCEPT
    {
        return get_value_dword_nothrow(key, nullptr, value_name, return_value);
    }

    inline HRESULT get_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ DWORD64* return_value) WI_NOEXCEPT
    {
        DWORD data_size_bytes{ sizeof * return_value };
        return HRESULT_FROM_WIN32(RegGetValueW(key, subkey, value_name, ::wil::reg::details::get_value_flags_from_value_type(REG_QWORD), nullptr, return_value, &data_size_bytes));
    }

    inline HRESULT get_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_ DWORD64* return_value) WI_NOEXCEPT
    {
        return get_value_qword_nothrow(key, nullptr, value_name, return_value);
    }

#if defined(__WIL_WINREG_STL)
    inline HRESULT get_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type, ::std::vector<BYTE>* data) WI_NOEXCEPT
        try
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        ::wil::reg::optional_value<::std::vector<BYTE>> registry_value{ regview.try_get_value<::std::vector<BYTE>>(subkey, value_name, type) };
        RETURN_IF_FAILED(registry_value.inner_error);

        ::std::swap(*data, registry_value.value);
        return S_OK;
    }
    CATCH_RETURN()

        inline HRESULT get_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR value_name, DWORD type, ::std::vector<BYTE>* data) WI_NOEXCEPT
    {
        return get_value_byte_vector_nothrow(key, nullptr, value_name, type, data);
    }

    inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, ::std::wstring* data) WI_NOEXCEPT try
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        ::wil::reg::optional_value<::std::wstring> registry_value{ regview.try_get_value<::std::wstring>(subkey, value_name) };
        RETURN_IF_FAILED(registry_value.inner_error);

        ::std::swap(*data, registry_value.value);
        return S_OK;
    }
    CATCH_RETURN()

        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, ::std::wstring* data) WI_NOEXCEPT
    {
        return get_value_string_nothrow(key, nullptr, value_name, data);
    }

    inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, ::std::wstring* data) WI_NOEXCEPT try
    {
        const reg_view_details::reg_view_nothrow regview{ key };
        ::wil::reg::optional_value<::std::wstring> registry_value{ regview.try_get_value<::std::wstring>(subkey, value_name, REG_EXPAND_SZ) };
        RETURN_IF_FAILED(registry_value.inner_error);

        ::std::swap(*data, registry_value.value);
        return S_OK;
    }
    CATCH_RETURN()

        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, ::std::wstring* data) WI_NOEXCEPT
    {
        return get_value_expanded_string_nothrow(key, nullptr, value_name, data);
    }

    inline HRESULT get_value_multistring_wstring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT try
    {
        return_value->clear();

        ::std::vector<BYTE> rawData;
        RETURN_IF_FAILED(get_value_byte_vector_nothrow(key, subkey, value_name, REG_MULTI_SZ, &rawData));

        if (!rawData.empty())
        {
            auto* const begin = reinterpret_cast<wchar_t*>(rawData.data());
            auto* const end = begin + rawData.size() / sizeof(wchar_t);
            *return_value = ::wil::reg::details::get_wstring_vector_from_multistring(::std::vector<wchar_t>(begin, end));
        }

        return S_OK;
    }
    CATCH_RETURN()

        inline HRESULT get_value_multistring_wstring_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT
    {
        return get_value_multistring_wstring_nothrow(key, nullptr, value_name, return_value);
    }

    inline ::wil::reg::optional_value<::std::vector<::std::wstring>> try_get_value_multistring_wstring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
    {
        ::std::vector<::std::wstring> local_value;
        const auto hr = get_value_multistring_wstring_nothrow(key, subkey, value_name, &local_value);
        if (SUCCEEDED(hr))
        {
            ::wil::reg::optional_value<::std::vector<::std::wstring>> return_value;
            return_value.value_assigned = true;
            ::std::swap(return_value.value, local_value);
            return return_value;
        }

        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            return {};
        }

        THROW_HR(hr);
    }

    inline ::wil::reg::optional_value<::std::vector<::std::wstring>> try_get_value_multistring_wstring(HKEY key, _In_opt_ PCWSTR value_name)
    {
        return try_get_value_multistring_wstring(key, nullptr, value_name);
    }
#endif // #if defined(__WIL_WINREG_STL)

} // namespace reg
} // namespace wil
#endif // __WIL_REGISTRY_BASIC_INCLUDED