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

// TODO: should we use feature checks instead of forced imports?
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <list>

#include <Windows.h>
#include <sddl.h>

#include "common.h"
#include "resource.h"

#ifdef _KERNEL_MODE
#error This header is not supported in kernel-mode.
#endif

#ifndef __WIL_WINREG_
#error This required wil::unique_hkey support
#endif

namespace wil
{
namespace registry
{
constexpr bool error_value_was_not_found(HRESULT hr) WI_NOEXCEPT
{
    return hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

constexpr bool error_value_was_too_large(HRESULT hr) WI_NOEXCEPT
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

    template <class InputIt>
    ::std::vector<wchar_t> get_multisz_from_wstrings(InputIt first, InputIt last)
    {
        ::std::vector<wchar_t> multiSz;

        if (first == last)
        {
            multiSz.push_back(L'\0');
            multiSz.push_back(L'\0');
            return multiSz;
        }

        for (const auto& wstr : ::wil::make_range(first, last))
        {
            if (!wstr.empty())
            {
                // back_inserter returns an iterator to enable ::std::copy to write
                // from begin() to end() at the back_inserter iterator, growing the vector as necessary
                ::std::copy(::std::begin(wstr), ::std::end(wstr), ::std::back_inserter(multiSz));
                // null terminator (1 of 2) or separator
                multiSz.push_back(L'\0');
            }
        }
        // null terminator (2 of 2)
        multiSz.push_back(L'\0');
        return multiSz;
    }

    inline ::std::vector<::std::wstring> get_wstring_vector_from_multisz(const ::std::vector<wchar_t>& data)
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
#endif

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

#ifdef WIL_ENABLE_EXCEPTIONS
    // These iterator classes facilitates STL iterator semantics to walk a set of subkeys and values
    // within an instantiated Key object. This can be directly constructed passing a unique_hkey
    // but will likely be more commonly constructed from reg_view_t::reg_enum_keys() and reg_view_t::reg_enum_values()
    template <typename T>
    class iterator_t;

    // The template implementation for registry iterator support 2 types - for keys and values
    // These typedefs are designed to simplify types for callers and mask the implementation details
    using key_iterator = iterator_t<::wil::registry::details::key_iterator_data>;
    using value_iterator = iterator_t<::wil::registry::details::value_iterator_data>;

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

        iterator_t(HKEY hkey, ::wil::registry::details::iterator_creation_flag type)
        {
            m_data.m_hkey = hkey;
            m_data.m_nextName.resize(type == ::wil::registry::details::iterator_creation_flag::begin ? ::wil::registry::details::iterator_default_buffer_size : 0);
            m_data.m_index = type == ::wil::registry::details::iterator_creation_flag::begin ? 0 : ::wil::registry::details::iterator_end_offset;

            if (type == ::wil::registry::details::iterator_creation_flag::begin)
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
            if (::wil::registry::details::iterator_end_offset == m_data.m_index)
            {
                THROW_WIN32(ERROR_NO_MORE_ITEMS);
            }
            return m_data.m_nextName.size() < 2 ? L"" : &m_data.m_nextName[0];
        }

        bool operator==(const iterator_t& rhs) const WI_NOEXCEPT
        {
            if (::wil::registry::details::iterator_end_offset == m_data.m_index || ::wil::registry::details::iterator_end_offset == rhs.m_data.m_index)
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
            if (newIndex == ::wil::registry::details::iterator_end_offset)
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
    inline void iterator_t<::wil::registry::details::key_iterator_data>::enumerate_next()
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
    inline void iterator_t<::wil::registry::details::value_iterator_data>::enumerate_next()
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
    inline DWORD iterator_t<::wil::registry::details::key_iterator_data>::type() const WI_NOEXCEPT
    {
        return REG_NONE;
    }

    template <>
    inline DWORD iterator_t<::wil::registry::details::value_iterator_data>::type() const WI_NOEXCEPT
    {
        return m_data.m_nextType;
    }
#endif
}

enum class optional_value_status
{
    no_value,
    has_value,
};

template <typename T>
struct optional_value
{
    constexpr bool has_value() const WI_NOEXCEPT
    {
        return type != optional_value_status::no_value;
    }

    constexpr explicit operator bool() const WI_NOEXCEPT
    {
        return has_value();
    }

    template <typename F>
    [[nodiscard]] constexpr const T& value_or(const F& f) const
    {
        return type != optional_value_status::no_value ? value : f;
    }

    T value{};
    optional_value_status type = optional_value_status::no_value;
};

namespace reg_view_details
{
    // generic functions that can apply to both specialized and non-specialized types
    // that are assigned into optional_value types

    /*template <typename T>
    constexpr void* get_buffer(const optional_value<T>& t) WI_NOEXCEPT
    {
        return const_cast<T*>(&t.value);
    }*/

    inline void* get_buffer(const std::vector<BYTE>& buffer) WI_NOEXCEPT
    {
        return const_cast<BYTE*>(buffer.data());
    }

    inline void* get_buffer(const ::std::wstring& string) WI_NOEXCEPT
    {
        return const_cast<wchar_t*>(string.data());
    }

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
    inline void* get_buffer(const wil::unique_bstr& value) WI_NOEXCEPT
    {
        return value.get();
    }

#if defined(__WIL_OLEAUTO_H_STL)
    inline void* get_buffer(const wil::shared_bstr& value) WI_NOEXCEPT
    {
        return value.get();
    }
#endif
#endif

    /*template <typename T>
    constexpr DWORD get_buffer_size(const optional_value<T>) WI_NOEXCEPT
    {
        return sizeof(T);
    }*/

    inline DWORD get_buffer_size(const std::vector<BYTE>& buffer) WI_NOEXCEPT
    {
        return static_cast<DWORD>(buffer.size());
    }

    inline DWORD get_buffer_size(const ::std::wstring& string) WI_NOEXCEPT
    {
        return static_cast<DWORD>(string.size() * sizeof(wchar_t));
    }

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

#if defined(__WIL_OLEAUTO_H_STL)
    inline DWORD get_buffer_size(const ::wil::shared_bstr& value) WI_NOEXCEPT
    {
        return ::SysStringLen(value.get());
    }
#endif
#endif

    // grow_buffer returns true if growing that buffer type is supported, false if growing that type is not supported
    // the caller will call get_buffer_size later to validate if the allocation succeeded

    template <typename T>
    constexpr bool grow_buffer(T&, DWORD) WI_NOEXCEPT
    {
        return false;
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    inline bool grow_buffer(std::vector<BYTE>& buffer, DWORD byteSize) WI_NOEXCEPT
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

    inline bool grow_buffer(::std::wstring& string, DWORD byteSize) WI_NOEXCEPT
    {
        try
        {
            // reset to size 0 in case resize() throws
            string.clear();
            string.resize(byteSize / 2);
        }
        catch (...)
        {
        }
        return true;
    }
#endif

#if defined(__WIL_OLEAUTO_H_)
    inline bool grow_buffer(::wil::unique_bstr& string, DWORD byteSize) WI_NOEXCEPT
    {
        string.reset(::SysAllocStringByteLen(nullptr, byteSize));
        if (string)
        {
            ZeroMemory(string.get(), byteSize);
        }
        return true;
    }

#if defined(__WIL_OLEAUTO_H_STL)
    inline bool grow_buffer(::wil::shared_bstr& string, DWORD byteSize) WI_NOEXCEPT
    {
        string.reset(::SysAllocStringByteLen(nullptr, byteSize));
        if (string)
        {
            ZeroMemory(string.get(), byteSize);
        }
        return true;
    }
#endif
#endif

    template <typename T>
    constexpr void trim_buffer(T&) WI_NOEXCEPT
    {
    }

    inline void trim_buffer(::std::wstring& buffer)
    {
        const auto offset = buffer.find_first_of(L'\0');
        if (offset != std::wstring::npos)
        {
            buffer.resize(offset);
        }
    }

    // get_value_type requires a well-known type for T
    template <typename T>
    DWORD get_value_type() WI_NOEXCEPT;

    template <>
    constexpr DWORD get_value_type<int32_t>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_DWORD);
    }

    template <>
    constexpr DWORD get_value_type<uint32_t>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_DWORD);
    }

    template <>
    constexpr DWORD get_value_type<long>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_DWORD);
    }

    template <>
    constexpr DWORD get_value_type<unsigned long>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_DWORD);
    }

    template <>
    constexpr DWORD get_value_type<int64_t>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_QWORD);
    }

    template <>
    constexpr DWORD get_value_type<uint64_t>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_QWORD);
    }

    template <>
    constexpr DWORD get_value_type<PCWSTR>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_SZ);
    }

    template <>
    constexpr DWORD get_value_type<::std::wstring>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_SZ);
    }
#if defined(__WIL_OLEAUTO_H_)
    template <>
    constexpr DWORD get_value_type<wil::unique_bstr>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_SZ);
    }

#if defined(__WIL_OLEAUTO_H_STL)
    template <>
    constexpr DWORD get_value_type<wil::shared_bstr>() WI_NOEXCEPT
    {
        return ::wil::registry::details::get_value_flags_from_value_type(REG_SZ);
    }
#endif
#endif

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

    constexpr DWORD set_value_type(const std::wstring&) WI_NOEXCEPT
    {
        return REG_SZ;
    }

    constexpr DWORD set_value_type(PCWSTR) WI_NOEXCEPT
    {
        return REG_SZ;
    }

    // type T is the owning type for storing a key: HKEY, wil::unique_hkey, wil::shared_hkey
    template <typename T, typename err_policy = err_exception_policy>
    class reg_view_t
    {
    public:
        template <typename K>
        explicit reg_view_t(K&& key) WI_NOEXCEPT :
            m_key(std::forward<K>(key))
        {
        }

        [[nodiscard]] HKEY get_key() const WI_NOEXCEPT
        {
            return ::wil::registry::details::get_key(m_key);
        }

        typename err_policy::result open_key(_In_opt_ PCWSTR subKey, ::wil::unique_hkey& hkey, ::wil::registry::key_access access = ::wil::registry::key_access::read) const
        {
            auto error = ::RegOpenKeyExW(::wil::registry::details::get_key(m_key), subKey, 0, ::wil::registry::details::get_access_flags(access), ::wil::out_param(hkey));
            if (error == ERROR_FILE_NOT_FOUND && !::wil::registry::details::should_return_not_found<err_policy>())
            {
                error = ERROR_SUCCESS;
                hkey.reset();
            }
            return err_policy::HResult(HRESULT_FROM_WIN32(error));
        }

#if defined(__WIL_WINREG_STL)
    typename err_policy::result open_key(_In_opt_ PCWSTR subKey, ::wil::shared_hkey& hkey, ::wil::registry::key_access access = ::wil::registry::key_access::read) const
    {
        auto error = ::RegOpenKeyExW(::wil::registry::details::get_key(m_key), subKey, 0, ::wil::registry::details::get_access_flags(access), ::wil::out_param(hkey));
        if (error == ERROR_FILE_NOT_FOUND && !::wil::registry::details::should_return_not_found<err_policy>())
        {
            error = ERROR_SUCCESS;
            hkey.reset();
        }
        return err_policy::HResult(HRESULT_FROM_WIN32(error));
    }
#endif

        ::wil::unique_hkey open_unique_key(_In_opt_ PCWSTR subKey, ::wil::registry::key_access access = ::wil::registry::key_access::read) const
        {
            ::wil::unique_hkey local_key{};
            open_key(subKey, local_key, access);
            return local_key;
        }

#if defined(__WIL_WINREG_STL)
    ::wil::shared_hkey open_shared_key(_In_opt_ PCWSTR subKey, ::wil::registry::key_access access = ::wil::registry::key_access::read) const
    {
        ::wil::shared_hkey local_key{};
        open_key(subKey, local_key, access);
        return local_key;
    }
#endif

        typename err_policy::result create_key(_In_ PCWSTR subKey, ::wil::unique_hkey& hkey, ::wil::registry::key_access access = ::wil::registry::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr) const
        {
            hkey.reset();

            const ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> securityDescriptor = ::wil::registry::details::create_security_descriptor<err_policy>(security_descriptor);

            SECURITY_ATTRIBUTES security_attributes{sizeof security_attributes, nullptr, FALSE};
            security_attributes.lpSecurityDescriptor = securityDescriptor.get();

            DWORD disposition = 0;
            const auto error =
                ::RegCreateKeyExW(::wil::registry::details::get_key(m_key), subKey, 0, nullptr, 0, ::wil::registry::details::get_access_flags(access), security_descriptor ? &security_attributes : nullptr, ::wil::out_param(hkey), &disposition);
            return err_policy::HResult(HRESULT_FROM_WIN32(error));
        }

#if defined(__WIL_WINREG_STL)
    typename err_policy::result create_key(_In_ PCWSTR subKey, ::wil::shared_hkey& hkey, ::wil::registry::key_access access = ::wil::registry::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr) const
    {
        hkey.reset();

        const ::wil::unique_hlocal_ptr<SECURITY_DESCRIPTOR> securityDescriptor = ::wil::registry::details::create_security_descriptor<err_policy>(security_descriptor);

        SECURITY_ATTRIBUTES security_attributes{sizeof security_attributes, nullptr, FALSE};
        security_attributes.lpSecurityDescriptor = securityDescriptor.get();

        DWORD disposition = 0;
        const auto error =
            ::RegCreateKeyExW(::wil::registry::details::get_key(m_key), subKey, 0, nullptr, 0, ::wil::registry::details::get_access_flags(access), security_descriptor ? &security_attributes : nullptr, ::wil::out_param(hkey), &disposition);
        return err_policy::HResult(HRESULT_FROM_WIN32(error));
    }
#endif

        ::wil::unique_hkey create_unique_key(_In_ PCWSTR subKey, ::wil::registry::key_access access = ::wil::registry::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr) const
        {
            ::wil::unique_hkey local_key{};
            create_key(subKey, local_key, access, security_descriptor);
            return local_key;
        }

#if defined(__WIL_WINREG_STL)
    ::wil::shared_hkey create_shared_key(_In_ PCWSTR subKey, ::wil::registry::key_access access = ::wil::registry::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr) const
    {
        ::wil::shared_hkey local_key{};
        create_key(subKey, local_key, access, security_descriptor);
        return local_key;
    }
#endif

        typename err_policy::result delete_key(_In_opt_ PCWSTR sub_key) WI_NOEXCEPT
        {
            auto error = RegDeleteTreeW(::wil::registry::details::get_key(m_key), sub_key);
            if (error == ERROR_FILE_NOT_FOUND)
            {
                error = ERROR_SUCCESS;
            }
            return err_policy::HResult(HRESULT_FROM_WIN32(error));
        }

        typename err_policy::result delete_value(HKEY key, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
        {
            return err_policy::HResult(HRESULT_FROM_WIN32(RegDeleteValueW(key, value_name)));
        }

    private:

        template <typename R, typename get_err_policy = err_policy>
        typename get_err_policy::result get_value_internal(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, R& return_value, DWORD type = ::wil::registry::reg_view_details::get_value_type<R>()) const
        {
            for (;;)
            {
                DWORD data_size_bytes{::wil::registry::reg_view_details::get_buffer_size(return_value)};
                auto error = ::RegGetValueW(
                    ::wil::registry::details::get_key(m_key),
                    subkey,
                    value_name,
                    ::wil::registry::details::get_value_flags_from_value_type(type),
                    nullptr,
                    ::wil::registry::reg_view_details::get_buffer(return_value),
                    &data_size_bytes);
                if (error == ERROR_SUCCESS)
                {
                    break;
                }
                if (error == ERROR_FILE_NOT_FOUND)
                {
                    // Simply pass on any not-found errors.
                    return get_err_policy::HResult(HRESULT_FROM_WIN32(error));
                }
                if (error == ERROR_MORE_DATA && ::wil::registry::reg_view_details::grow_buffer(return_value, data_size_bytes))
                {
                    // verify if grow_buffer succeeded allocation
                    if (::wil::registry::reg_view_details::get_buffer_size(return_value) == 0)
                    {
                        error = ERROR_NOT_ENOUGH_MEMORY;
                        break;
                    }
                    continue;
                }

                return get_err_policy::HResult(HRESULT_FROM_WIN32(error));
            }

            ::wil::registry::reg_view_details::trim_buffer(return_value);
            return get_err_policy::HResult(S_OK);
        }

    public:

        template <typename R>
        typename err_policy::result get_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, R& return_value, DWORD type = ::wil::registry::reg_view_details::get_value_type<R>()) const
        {
            return get_value_internal(subkey, value_name, return_value, type);
        }

        template <typename R>
        typename err_policy::result get_value(_In_opt_ PCWSTR value_name, R& return_value, DWORD type = ::wil::registry::reg_view_details::get_value_type<R>()) const
        {
            return get_value(nullptr, value_name, return_value, type);
        }

        // intended for err_exception_policy as err_returncode_policy will not get an error code
        template <typename R>
        R get_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type = ::wil::registry::reg_view_details::get_value_type<R>()) const
        {
            R value{};
            get_value(subkey, value_name, value, type);
            return value;
        }

        // intended for err_exception_policy as err_returncode_policy will not get an error code
        template <typename R>
        R get_value(_In_opt_ PCWSTR value_name, DWORD type = ::wil::registry::reg_view_details::get_value_type<R>()) const
        {
            return get_value(nullptr, value_name, type);
        }

        template <typename R>
        typename err_policy::result try_get_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, optional_value<R>& return_value, DWORD type = ::wil::registry::reg_view_details::get_value_type<R>()) const
        {
            const auto hr = get_value_internal<R, err_returncode_policy>(subkey, value_name, return_value.value, type);
            if (SUCCEEDED(hr))
            {
                return_value.type = optional_value_status::has_value;
                // TODO: set value

                return err_policy::HResult(S_OK);
            } else if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                return_value.type = optional_value_status::no_value;
                // TODO: reset value.

                return err_policy::HResult(S_OK);
            }

            return err_policy::HResult(HRESULT_FROM_WIN32(hr));
        }

        template <typename R>
        typename err_policy::result try_get_value(_In_opt_ PCWSTR value_name, optional_value<R>& return_value, DWORD type = ::wil::registry::reg_view_details::get_value_type<R>()) const
        {
            return try_get_value(nullptr, value_name, return_value, type);
        }

        // intended for err_exception_policy as err_returncode_policy will not get an error code
        template <typename R>
        R try_get_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type = ::wil::registry::reg_view_details::get_value_type<R>()) const
        {
            R value{};
            try_get_value(subkey, value_name, value, type);
            return value;
        }

        // intended for err_exception_policy as err_returncode_policy will not get an error code
        template <typename R>
        R try_get_value(_In_opt_ PCWSTR value_name, DWORD type = ::wil::registry::reg_view_details::get_value_type<R>()) const
        {
            return try_get_value(nullptr, value_name, type);
        }

        template <typename R>
        typename err_policy::result set_value(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const R& value) const
        {
            const auto error = ::RegSetKeyValueW(
                ::wil::registry::details::get_key(m_key),
                subkey,
                value_name,
                ::wil::registry::reg_view_details::set_value_type(value),
                static_cast<BYTE*>(::wil::registry::reg_view_details::get_buffer(value)),
                ::wil::registry::reg_view_details::get_buffer_size(value));
            return err_policy::HResult(HRESULT_FROM_WIN32(error));
        }

        // TODO: just call set_value with nullptr subkey?
        template <typename R>
        typename err_policy::result set_value(_In_opt_ PCWSTR value_name, const R& value) const
        {
            const auto error = RegSetValueExW(
                ::wil::registry::details::get_key(m_key),
                value_name,
                0,
                ::wil::registry::reg_view_details::set_value_type(value),
                static_cast<BYTE*>(::wil::registry::reg_view_details::get_buffer(value)),
                ::wil::registry::reg_view_details::get_buffer_size(value));
            return err_policy::HResult(HRESULT_FROM_WIN32(error));
        }

#ifdef WIL_ENABLE_EXCEPTIONS
        template <typename R>
        typename err_policy::result set_value_multisz(_In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const R& data) const try
        {
            const auto multiSzWcharVector(::wil::registry::details::get_multisz_from_wstrings(::std::begin(data), ::std::end(data)));

            const auto* byteArray = reinterpret_cast<const BYTE*>(&multiSzWcharVector[0]);
            const auto byteArrayLength = static_cast<DWORD>(multiSzWcharVector.size() * sizeof(wchar_t));
            const auto error = ::RegSetKeyValueW(
                ::wil::registry::details::get_key(m_key),
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

        // TODO: just call set_value_multisz with nullptr subkey?
        template <typename R>
        typename err_policy::result set_value_multisz(_In_opt_ PCWSTR value_name, const R& data) const try
        {
            const auto multiSzWcharVector(::wil::registry::details::get_multisz_from_wstrings(::std::begin(data), ::std::end(data)));

            const auto* byteArray = reinterpret_cast<const BYTE*>(&multiSzWcharVector[0]);
            const auto byteArrayLength = static_cast<DWORD>(multiSzWcharVector.size() * sizeof(wchar_t));
            const auto error = RegSetValueExW(
                ::wil::registry::details::get_key(m_key),
                value_name,
                0,
                REG_MULTI_SZ,
                byteArray,
                byteArrayLength);
            return err_policy::HResult(HRESULT_FROM_WIN32(error));
        }
        catch (...)
        {
            return err_policy::HResult(::wil::ResultFromCaughtException());
        }

        struct key_enumerator
        {
            explicit key_enumerator(HKEY key) :
                m_hkey(key)
            {
            }

            [[nodiscard]] ::wil::registry::details::key_iterator begin() const
            {
                return ::wil::registry::details::key_iterator(m_hkey, ::wil::registry::details::iterator_creation_flag::begin);
            }

            [[nodiscard]] ::wil::registry::details::key_iterator end() const WI_NOEXCEPT
            {
                return ::wil::registry::details::key_iterator(m_hkey, ::wil::registry::details::iterator_creation_flag::end);
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
            [[nodiscard]] ::wil::registry::details::value_iterator begin() const
            {
                return ::wil::registry::details::value_iterator(m_hkey, ::wil::registry::details::iterator_creation_flag::begin);
            }

            [[nodiscard]] ::wil::registry::details::value_iterator end() const WI_NOEXCEPT
            {
                return ::wil::registry::details::value_iterator(m_hkey, ::wil::registry::details::iterator_creation_flag::end);
            }

        private:
            friend class reg_view_t;
            // non-owning resource (just get a copy from the owner)
            HKEY m_hkey;
        };

        [[nodiscard]] key_enumerator reg_enum_keys() const
        {
            return key_enumerator(::wil::registry::details::get_key(m_key));
        }

        [[nodiscard]] value_enumerator reg_enum_values() const
        {
            return value_enumerator(::wil::registry::details::get_key(m_key));
        }
#endif

    private:
        T m_key{};
    };

    template <typename T, typename err_policy = err_exception_policy>
    static reg_view_t<T, err_policy> create_reg_view(T&& t)
    {
        return reg_view_t<T, err_policy>(std::forward<T>(t));
    }

    // reg_view with the raw HKEY type is non-owning
    using reg_view_nothrow = reg_view_details::reg_view_t<HKEY, ::wil::err_returncode_policy>;
    using reg_view_unique_hkey_nothrow = reg_view_details::reg_view_t<::wil::unique_hkey, ::wil::err_returncode_policy>;
#if defined(__WIL_WINREG_STL)
    using reg_view_shared_hkey_nothrow = reg_view_details::reg_view_t<::wil::shared_hkey, ::wil::err_returncode_policy>;
#endif

#ifdef WIL_ENABLE_EXCEPTIONS
    // reg_view with the raw HKEY type is non-owning
    using reg_view = reg_view_details::reg_view_t<HKEY, ::wil::err_exception_policy>;
    using reg_view_unique_hkey = reg_view_details::reg_view_t<::wil::unique_hkey, ::wil::err_exception_policy>;
#if defined(__WIL_WINREG_STL)
    using reg_view_shared_hkey = reg_view_details::reg_view_t<::wil::shared_hkey, ::wil::err_exception_policy>;
#endif
#endif
}

// Registry Open* and Create* functions, both throwing and non-throwing

#ifdef WIL_ENABLE_EXCEPTIONS
inline ::wil::unique_hkey open_unique_key(HKEY key, _In_opt_ PCWSTR path, ::wil::registry::key_access access = ::wil::registry::key_access::read)
{
    const reg_view_details::reg_view regview{key};
    return regview.open_unique_key(path, access);
}

inline ::wil::unique_hkey create_unique_key(HKEY key, _In_ PCWSTR path, ::wil::registry::key_access access = ::wil::registry::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
{
    const reg_view_details::reg_view regview{key};
    return regview.create_unique_key(path, access, security_descriptor);
}

#if defined(__WIL_WINREG_STL)
inline ::wil::shared_hkey open_shared_key(HKEY key, _In_opt_ PCWSTR path, ::wil::registry::key_access access = ::wil::registry::key_access::read)
{
    const reg_view_details::reg_view regview{key};
    return regview.open_shared_key(path, access);
}

inline ::wil::shared_hkey create_shared_key(HKEY key, _In_ PCWSTR path, ::wil::registry::key_access access = ::wil::registry::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
{
    const reg_view_details::reg_view regview{key};
    return regview.create_shared_key(path, access, security_descriptor);
}
#endif
#endif

inline HRESULT open_key_nothrow(HKEY key, _In_opt_ PCWSTR path, ::wil::unique_hkey& hkey, ::wil::registry::key_access access = ::wil::registry::key_access::read) WI_NOEXCEPT
{
    const reg_view_details::reg_view_nothrow regview{key};
    return regview.open_key(path, hkey, access);
}

#if defined(__WIL_WINREG_STL)
inline HRESULT open_key_nothrow(HKEY key, _In_opt_ PCWSTR path, ::wil::shared_hkey& hkey, ::wil::registry::key_access access = ::wil::registry::key_access::read) WI_NOEXCEPT
{
    const reg_view_details::reg_view_nothrow regview{key};
    return regview.open_key(path, hkey, access);
}
#endif

inline HRESULT create_key_nothrow(HKEY key, _In_ PCWSTR path, ::wil::unique_hkey& hkey, ::wil::registry::key_access access = ::wil::registry::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
{
    const reg_view_details::reg_view_nothrow regview{key};
    return regview.create_key(path, hkey, access, security_descriptor);
}

#if defined(__WIL_WINREG_STL)
inline HRESULT create_key_nothrow(HKEY key, _In_ PCWSTR path, ::wil::shared_hkey& hkey, ::wil::registry::key_access access = ::wil::registry::key_access::read, _In_opt_ PCWSTR security_descriptor = nullptr)
{
    const reg_view_details::reg_view_nothrow regview{key};
    return regview.create_key(path, hkey, access, security_descriptor);
}
#endif

#ifdef WIL_ENABLE_EXCEPTIONS
inline size_t get_child_key_count(HKEY key)
{
    DWORD numSubKeys{};
    THROW_IF_WIN32_ERROR(::RegQueryInfoKeyW(
        key, nullptr, nullptr, nullptr, &numSubKeys, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    return numSubKeys;
}

inline size_t reg_get_child_value_count(HKEY key)
{
    DWORD numSubValues{};
    THROW_IF_WIN32_ERROR(::RegQueryInfoKeyW(
        key, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, &numSubValues, nullptr, nullptr, nullptr, nullptr));
    return numSubValues;
}
#endif

inline HRESULT get_child_key_count_nothrow(HKEY key, _Out_ DWORD* numSubKeys) WI_NOEXCEPT
{
    RETURN_IF_WIN32_ERROR(::RegQueryInfoKeyW(
        key, nullptr, nullptr, nullptr, numSubKeys, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    return S_OK;
}

inline HRESULT reg_get_child_value_count_nothrow(HKEY key, _Out_ DWORD* numSubValues) WI_NOEXCEPT
{
    RETURN_IF_WIN32_ERROR(::RegQueryInfoKeyW(
        key, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, numSubValues, nullptr, nullptr, nullptr, nullptr));
    return S_OK;
}

// Registry Set* functions, both throwing and non-throwing

#ifdef WIL_ENABLE_EXCEPTIONS
inline void set_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD data)
{
    const reg_view_details::reg_view regview{key};
    return regview.set_value(subkey, value_name, data);
}

inline void set_value_dword(HKEY key, _In_opt_ PCWSTR value_name, DWORD data)
{
    return set_value_dword(key, nullptr, value_name, data);
}

inline void set_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint64_t data)
{
    const reg_view_details::reg_view regview{key};
    return regview.set_value(subkey, value_name, data);
}

inline void set_value_qword(HKEY key, _In_opt_ PCWSTR value_name, uint64_t data)
{
    return set_value_qword(key, nullptr, value_name, data);
}

inline void set_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ PCWSTR data)
{
    const reg_view_details::reg_view regview{key};
    return regview.set_value(subkey, value_name, data);
}

inline void set_value_string(HKEY key, _In_opt_ PCWSTR value_name, _In_ PCWSTR data)
{
    return set_value_string(key, nullptr, value_name, data);
}

inline void set_value_multisz(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
{
    const reg_view_details::reg_view regview{key};
    return regview.set_value_multisz(subkey, value_name, data);
}

inline void set_value_multisz(HKEY key, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
{
    return set_value_multisz(key, nullptr, value_name, data);
}

inline void set_value_multisz(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const ::std::list<::std::wstring>& data)
{
    const reg_view_details::reg_view regview{key};
    return regview.set_value_multisz(subkey, value_name, data);
}

inline void set_value_multisz(HKEY key, _In_opt_ PCWSTR value_name, const ::std::list<::std::wstring>& data)
{
    return set_value_multisz(key, nullptr, value_name, data);
}
#endif

inline HRESULT set_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD data) WI_NOEXCEPT
{
    const reg_view_details::reg_view_nothrow regview{key};
    return regview.set_value(subkey, value_name, data);
}

inline HRESULT set_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, DWORD data) WI_NOEXCEPT
{
    return set_value_dword_nothrow(key, nullptr, value_name, data);
}

inline HRESULT set_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint64_t data) WI_NOEXCEPT
{
    const reg_view_details::reg_view_nothrow regview{key};
    return regview.set_value(subkey, value_name, data);
}

inline HRESULT set_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, uint64_t data) WI_NOEXCEPT
{
    return set_value_qword_nothrow(key, nullptr, value_name, data);
}

inline HRESULT set_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
{
    const reg_view_details::reg_view_nothrow regview{key};
    return regview.set_value(subkey, value_name, data);
}

inline HRESULT set_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_ PCWSTR data) WI_NOEXCEPT
{
    return set_value_string_nothrow(key, nullptr, value_name, data);
}

// TODO: write multisz setters that don't throw.
//inline HRESULT set_value_multisz_nothrow(HKEY key, _In_ PCWSTR value_name, const ::std::vector<::std::wstring>& data) WI_NOEXCEPT
//{
//    const reg_view_details::reg_view_nothrow regview{key};
//    return regview.set_value_multisz(value_name, data);
//}
//
//inline HRESULT set_value_multisz_nothrow(HKEY key, _In_ PCWSTR value_name, const ::std::list<::std::wstring>& data) WI_NOEXCEPT
//{
//    const reg_view_details::reg_view_nothrow regview{key};
//    return regview.set_value_multisz(value_name, data);
//}

//  Registry get* functions, both throwing and non-throwing

#ifdef WIL_ENABLE_EXCEPTIONS
inline DWORD get_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    const reg_view_details::reg_view regview{key};
    return regview.get_value<DWORD>(subkey, value_name);
}

inline DWORD get_value_dword(HKEY key, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    return get_value_dword(key, nullptr, value_name);
}

inline optional_value<DWORD> try_get_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    const reg_view_details::reg_view regview{key};
    optional_value<DWORD> value;
    regview.try_get_value<DWORD>(subkey, value_name, value);
    return value;
}

inline optional_value<DWORD> try_get_value_dword(HKEY key, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    return try_get_value_dword(key, nullptr, value_name);
}

inline DWORD64 get_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    const reg_view_details::reg_view regview{ key };
    return regview.get_value<DWORD64>(subkey, value_name);
}

inline DWORD64 get_value_qword(HKEY key, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    return get_value_qword(key, nullptr, value_name);
}

inline optional_value<DWORD64> try_get_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    const reg_view_details::reg_view regview{key};
    optional_value<DWORD64> value;
    regview.try_get_value<DWORD64>(subkey, value_name, value);
    return value;
}

inline optional_value<DWORD64> try_get_value_qword(HKEY key, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    return try_get_value_qword(key, nullptr, value_name);
}

inline ::std::vector<BYTE> get_value_byte_vector(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type) WI_NOEXCEPT
{
    const reg_view_details::reg_view regview{ key };
    return regview.get_value<::std::vector<BYTE>>(subkey, value_name, type);
}

inline ::std::vector<BYTE> get_value_byte_vector(HKEY key, _In_opt_ PCWSTR value_name, DWORD type) WI_NOEXCEPT
{
    return get_value_byte_vector(key, nullptr, value_name, type);
}

inline optional_value<::std::vector<BYTE>> try_get_value_byte_vector(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type)
{
    const reg_view_details::reg_view regview{key};
    optional_value<::std::vector<BYTE>> value;
    regview.try_get_value<::std::vector<BYTE>>(subkey, value_name, value, type);
    return value;
}

inline optional_value<::std::vector<BYTE>> try_get_value_byte_vector(HKEY key, _In_opt_ PCWSTR value_name, DWORD type)
{
    return try_get_value_byte_vector(key, nullptr, value_name, type);
}

inline ::std::wstring get_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    const reg_view_details::reg_view regview{ key };
    return regview.get_value<::std::wstring>(subkey, value_name);
}

inline ::std::wstring get_value_string(HKEY key, _In_opt_ PCWSTR value_name) WI_NOEXCEPT
{
    return get_value_string(key, nullptr, value_name);
}

inline optional_value<::std::wstring> try_get_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
{
    const reg_view_details::reg_view regview{key};
    optional_value<::std::wstring> optionalvalue;
    regview.try_get_value<::std::wstring>(subkey, value_name, optionalvalue);
    return optionalvalue;
}

inline optional_value<::std::wstring> try_get_value_string(HKEY key, _In_opt_ PCWSTR value_name)
{
    return try_get_value_string(key, nullptr, value_name);
}

inline ::std::wstring get_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
{
    const reg_view_details::reg_view regview{ key };
    ::std::wstring value;
    regview.get_value<::std::wstring>(subkey, value_name, value, REG_EXPAND_SZ);
    return value;
}

inline ::std::wstring get_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name)
{
    return get_value_expanded_string(key, nullptr, value_name);
}

inline optional_value<::std::wstring> try_get_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
{
    const reg_view_details::reg_view regview{key};
    optional_value<::std::wstring> optionalvalue;
    regview.try_get_value<::std::wstring>(subkey, value_name, optionalvalue, REG_EXPAND_SZ);
    return optionalvalue;
}

inline optional_value<::std::wstring> try_get_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name)
{
    return try_get_value_expanded_string(key, nullptr, value_name);
}
#endif

inline HRESULT get_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ DWORD* return_value) WI_NOEXCEPT
{
    DWORD data_size_bytes{sizeof *return_value};
    return HRESULT_FROM_WIN32(RegGetValueW(key, subkey, value_name, ::wil::registry::details::get_value_flags_from_value_type(REG_DWORD), nullptr, return_value, &data_size_bytes));
}

inline HRESULT get_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_ DWORD* return_value) WI_NOEXCEPT
{
    return get_value_dword_nothrow(key, nullptr, value_name, return_value);
}

inline HRESULT get_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _In_ DWORD64* return_value) WI_NOEXCEPT
{
    DWORD data_size_bytes{sizeof *return_value};
    return HRESULT_FROM_WIN32(RegGetValueW(key, subkey, value_name, ::wil::registry::details::get_value_flags_from_value_type(REG_QWORD), nullptr, return_value, &data_size_bytes));
}

inline HRESULT get_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _In_ DWORD64* return_value) WI_NOEXCEPT
{
    return get_value_qword_nothrow(key, nullptr, value_name, return_value);
}

// TODO: write value_byte_vector getters that don't throw.
//inline HRESULT get_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type, ::std::vector<BYTE>& data) WI_NOEXCEPT
//{
//    const reg_view_details::reg_view_nothrow regview{key};
//    optional_value<::std::vector<BYTE>> optionalvalue;
//    const auto hr = regview.try_get_value<::std::vector<BYTE>>(subkey, value_name, optionalvalue, type);
//    RETURN_IF_FAILED(hr);
//
//    if (optionalvalue.type != optional_value_status::has_value)
//    {
//        RETURN_WIN32(ERROR_FILE_NOT_FOUND);
//    }
//
//    std::swap(data, optionalvalue.value);
//    return S_OK;
//}
//
//inline HRESULT get_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR value_name, DWORD type, ::std::vector<BYTE>& data) WI_NOEXCEPT
//{
//    return get_value_byte_vector_nothrow(key, nullptr, value_name, type, data);
//}

#if defined(__WIL_STL_INCLUDED) && defined(WIL_ENABLE_EXCEPTIONS)
inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, ::std::wstring& data) WI_NOEXCEPT try
{
    const reg_view_details::reg_view_nothrow regview{key};
    optional_value<::std::wstring> optionalvalue;
    const auto hr = regview.try_get_value<::std::wstring>(subkey, value_name, optionalvalue);
    RETURN_IF_FAILED(hr);

    if (optionalvalue.type != optional_value_status::has_value)
    {
        RETURN_WIN32(ERROR_FILE_NOT_FOUND);
    }

    std::swap(data, optionalvalue.value);
    return S_OK;
}
CATCH_RETURN()

inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, ::std::wstring& data) WI_NOEXCEPT
{
    return get_value_string_nothrow(key, nullptr, value_name, data);
}

inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, ::std::wstring& data) WI_NOEXCEPT try
{
    const reg_view_details::reg_view_nothrow regview{key};
    optional_value<::std::wstring> optionalvalue;
    const auto hr = regview.try_get_value<::std::wstring>(subkey, value_name, optionalvalue, REG_EXPAND_SZ);
    RETURN_IF_FAILED(hr);

    if (optionalvalue.type != optional_value_status::has_value)
    {
        RETURN_WIN32(ERROR_FILE_NOT_FOUND);
    }

    std::swap(data, optionalvalue.value);
    return S_OK;
}
CATCH_RETURN()

inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, ::std::wstring& data) WI_NOEXCEPT
{
    return get_value_expanded_string_nothrow(key, nullptr, value_name, data);
}

inline HRESULT get_value_multistring_wstring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, ::std::vector<::std::wstring>* return_value) WI_NOEXCEPT try
{
    return_value->clear();

    ::std::vector<BYTE> rawData;
    RETURN_IF_FAILED(get_value_byte_vector_nothrow(key, subkey, value_name, REG_MULTI_SZ, rawData));
    if (!rawData.empty())
    {
        auto* const begin = reinterpret_cast<wchar_t*>(rawData.data());
        auto* const end = begin + rawData.size() / sizeof(wchar_t);
        *return_value = ::wil::registry::details::get_wstring_vector_from_multisz(::std::vector<wchar_t>(begin, end));
    }
    return S_OK;
}
CATCH_RETURN()

inline HRESULT get_value_multistring_wstring_nothrow(HKEY key, _In_opt_ PCWSTR value_name, ::std::vector<::std::wstring>* return_value) WI_NOEXCEPT try
{
    return get_value_multistring_wstring_nothrow(key, nullptr, value_name, return_value);
}

inline optional_value<::std::vector<::std::wstring>> get_value_multistring_wstring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
{
    ::std::vector<::std::wstring> local_value;
    const auto hr = get_value_multistring_wstring_nothrow(key, subkey, value_name, &local_value);
    if (SUCCEEDED(hr))
    {
        optional_value<::std::vector<::std::wstring>> return_value;
        return_value.type = optional_value_status::has_value;
        std::swap(return_value.value, local_value);
        return return_value;
    }
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        return {};
    }
    THROW_HR(hr);
}

inline optional_value<::std::vector<::std::wstring>> get_value_multistring_wstring(HKEY key, _In_opt_ PCWSTR value_name)
{
    return get_value_multistring_wstring(key, nullptr, value_name);
}

#endif

template <size_t Length>
HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, wchar_t return_value[Length], _Out_opt_ DWORD* pRequiredBytes = nullptr) WI_NOEXCEPT
{
    // don't allocate, just use their buffer
    DWORD data_size_bytes{Length * sizeof(wchar_t)};
    const auto error = RegGetValueW(key, subkey, value_name, ::wil::registry::details::get_value_flags_from_value_type(REG_SZ), nullptr, return_value, &data_size_bytes);
    if (pRequiredBytes && error == ERROR_MORE_DATA)
    {
        *pRequiredBytes = data_size_bytes;
    }
    return HRESULT_FROM_WIN32(error);
}

template <size_t Length>
HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, wchar_t return_value[Length], _Out_opt_ DWORD* pRequiredBytes = nullptr) WI_NOEXCEPT
{
    return get_value_string_nothrow(key, nullptr, value_name, return_value, pRequiredBytes);
}
} // namespace registry
} // namespace wil
#endif // __WIL_REGISTRY_BASIC_INCLUDED