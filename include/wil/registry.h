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
#ifndef __WIL_REGISTRY_INCLUDED
#define __WIL_REGISTRY_INCLUDED

#ifdef _KERNEL_MODE
#error This header is not supported in kernel-mode.
#endif

#include <winreg.h>
#include <new.h> // new(std::nothrow)
#include "registry_helpers.h"
#include "resource.h"

#ifndef __WIL_WINREG_
#error __WIL_WINREG_ is required for ::wil::unique_hkey support; ensure the raw registry APIs (like RegCreateKeyExA) are available
#endif

// wil registry does not require the use of the STL or C++ exceptions (see _nothrow functions)
// wil registry natively supports std::vector and std::wstring when preferring those types
// wil registry uses the __WIL_WINREG_STL define to enable support for wil::shared_* types (defined in resource.h)

namespace wil
{
    namespace reg
    {
        /**
         * \brief Helper function to translate registry return values if the value was not found
         * \param hr HRESULT to test from registry APIs
         * \return boolean if the HRESULT indicates the registry value was not found
         */
        constexpr bool is_hresult_not_found(HRESULT hr) WI_NOEXCEPT
        {
            return hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        /**
         * \brief Helper function to translate registry return values if the buffer was too small
         * \param hr HRESULT to test from registry APIs
         * \return boolean if the HRESULT indicates the buffer was too small for the value being read
         */
        constexpr bool is_hresult_buffer_too_small(HRESULT hr) WI_NOEXCEPT
        {
            return hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA);
        }

#if defined(WIL_ENABLE_EXCEPTIONS)
        /**
         * \brief Opens a new HKEY to the specified path - see RegOpenKeyExW
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param path A string to append to the HKEY to attempt to open
         *        can be nullptr if not needed
         * \param access The requested access desired for the opened key
         * \return A wil::unique_hkey containing the resulting opened HKEY
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline ::wil::unique_hkey open_unique_key(HKEY key, _In_opt_ PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            ::wil::unique_hkey return_value;
            regview.open_key(path, &return_value, access);
            return return_value;
        }

        /**
         * \brief Creates a new HKEY to the specified path - see RegCreateKeyExW
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param path A string to append to the HKEY to attempt to open
         *        can be nullptr if not needed
         * \param access The requested access desired for the opened key
         * \return A wil::unique_hkey or wil::shared_hkey containing the resulting opened HKEY
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline ::wil::unique_hkey create_unique_key(HKEY key, PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            ::wil::unique_hkey return_value;
            regview.create_key(path, &return_value, access);
            return return_value;
        }

#if defined(__WIL_WINREG_STL)
        /**
         * \brief Opens a new HKEY to the specified path - see RegOpenKeyExW
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param path A string to append to the HKEY to attempt to open
         *        can be nullptr if not needed
         * \param access The requested access desired for the opened key
         * \return A wil::shared_hkey containing the resulting opened HKEY
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline ::wil::shared_hkey open_shared_key(HKEY key, PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            ::wil::shared_hkey return_value;
            regview.open_key(path, &return_value, access);
            return return_value;
        }

        /**
         * \brief Creates a new HKEY to the specified path - see RegCreateKeyExW
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param path A string to append to the HKEY to attempt to open
         *        can be nullptr if not needed
         * \param access The requested access desired for the opened key
         * \return A wil::shared_hkey or wil::shared_hkey containing the resulting opened HKEY
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline ::wil::shared_hkey create_shared_key(HKEY key, PCWSTR path, ::wil::reg::key_access access = ::wil::reg::key_access::read)
        {
            const reg_view_details::reg_view regview{ key };
            ::wil::shared_hkey return_value;
            regview.create_key(path, &return_value, access);
            return return_value;
        }
#endif // #if defined(__WIL_WINREG_STL)
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

        /**
         * \brief Opens a new HKEY to the specified path - see RegOpenKeyExW
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param path A string to append to the HKEY to attempt to open
         *        can be nullptr if not needed
         * \param[out] hkey A reference to a wil::unique_hkey to receive the opened HKEY
         * \param access The requested access desired for the opened key
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT open_unique_key_nothrow(HKEY key, _In_opt_ PCWSTR path, _Out_::wil::unique_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
        {
            hkey.reset();
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.open_key(path, hkey.addressof(), access);
        }

        /**
         * \brief Creates a new HKEY to the specified path - see RegCreateKeyExW
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param path A string to append to the HKEY to attempt to open
         *        can be nullptr if not needed
         * \param[out] hkey A reference to a wil::unique_hkey to receive the opened HKEY
         * \param access The requested access desired for the opened key
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT create_unique_key_nothrow(HKEY key, PCWSTR path, _Out_::wil::unique_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
        {
            hkey.reset();
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.create_key(path, hkey.addressof(), access);
        }

#if defined(__WIL_WINREG_STL)
        /**
         * \brief Opens a new HKEY to the specified path - see RegOpenKeyExW
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param path A string to append to the HKEY to attempt to open
         *        can be nullptr if not needed
         * \param[out] hkey A reference to a wil::shared_hkey to receive the opened HKEY
         * \param access The requested access desired for the opened key
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT open_shared_key_nothrow(HKEY key, _In_opt_ PCWSTR path, _Out_::wil::shared_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
        {
            hkey.reset();
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.open_key(path, hkey.addressof(), access);
        }

        /**
         * \brief Creates a new HKEY to the specified path - see RegCreateKeyExW
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param path A string to append to the HKEY to attempt to open
         *        can be nullptr if not needed
         * \param[out] hkey A reference to a wil::shared_hkey to receive the opened HKEY
         * \param access The requested access desired for the opened key
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT create_shared_key_nothrow(HKEY key, _In_opt_ PCWSTR path, _Out_::wil::shared_hkey& hkey, ::wil::reg::key_access access = ::wil::reg::key_access::read) WI_NOEXCEPT
        {
            hkey.reset();
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.create_key(path, hkey.addressof(), access);
        }
#endif // #define __WIL_WINREG_STL

#if defined(WIL_ENABLE_EXCEPTIONS)
        /**
         * \brief Queries for number of sub-keys
         * \param key The HKEY to query for number of sub-keys
         * \return The queried number of sub-keys if succeeded
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline size_t get_child_key_count(HKEY key)
        {
            DWORD numSubKeys{};
            THROW_IF_WIN32_ERROR(::RegQueryInfoKeyW(
                key, nullptr, nullptr, nullptr, &numSubKeys, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
            return numSubKeys;
        }

        /**
         * \brief Queries for number of values
         * \param key The HKEY to query for number of values
         * \return The queried number of value if succeeded
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline size_t get_child_value_count(HKEY key)
        {
            DWORD numSubValues{};
            THROW_IF_WIN32_ERROR(::RegQueryInfoKeyW(
                key, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, &numSubValues, nullptr, nullptr, nullptr, nullptr));
            return numSubValues;
        }
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

        /**
         * \brief Queries for number of sub-keys
         * \param key The HKEY to query for number of sub-keys
         * \param[out] numSubKeys A pointer to a DWORD to receive the returned count
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_child_key_count_nothrow(HKEY key, _Out_ DWORD* numSubKeys) WI_NOEXCEPT
        {
            RETURN_IF_WIN32_ERROR(::RegQueryInfoKeyW(
                key, nullptr, nullptr, nullptr, numSubKeys, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
            return S_OK;
        }

        /**
         * \brief Queries for number of values
         * \param key The HKEY to query for number of sub-keys
         * \param[out] numSubValues A pointer to a DWORD to receive the returned count
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_child_value_count_nothrow(HKEY key, _Out_ DWORD* numSubValues) WI_NOEXCEPT
        {
            RETURN_IF_WIN32_ERROR(::RegQueryInfoKeyW(
                key, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, numSubValues, nullptr, nullptr, nullptr, nullptr));
            return S_OK;
        }

#if defined(WIL_ENABLE_EXCEPTIONS)
        //
        // template <typename T>
        // void set_value(...)
        //
        //  - Writes a value under a specified key, the registry type based off the templated type passed as data
        //  - Throws a std::exception on failure (including wil::ResultException)
        //
        // Examples of usage (the template type does not need to be explicitly specified)
        //     wil::reg::set_value(key, L"subkey", L"dword_value_name", 0); // writes a REG_DWORD
        //     wil::reg::set_value(key, L"subkey", L"qword_value_name", 0ull); // writes a REG_QWORD
        //     wil::reg::set_value(key, L"subkey", L"string_value_name", L"hello"); // writes a REG_SZ
        //
        // A subkey is not required if the key is opened where this should write the value:
        //     wil::reg::set_value(key, L"dword_value_name", 0); // writes a REG_DWORD
        //     wil::reg::set_value(key, L"qword_value_name", 0ull); // writes a REG_QWORD
        //     wil::reg::set_value(key, L"string_value_name", L"hello"); // writes a REG_SZ
        //
        // Example usage writing a vector of wstrings to a REG_MULTI_SZ
        //     std::vector<std::wstring> data { L"string1", L"string2", L"string3" };
        //     wil::reg::set_value(key, L"multi_string_value_name", data);
        //     wil::reg::set_value(key, L"multi_string_value_name", data);
        //
        // Example of usage writing directly to a registry value from a raw byte vector
        //  - notice the registry type is required, not implied
        //     std::vector<BYTE> data { 0x00, 0xff, 0xee, 0xdd, 0xcc };
        //     wil::reg::set_value_byte_vector(key, L"binary_value_name", REG_BINARY, data);
        //     wil::reg::set_value_byte_vector(key, L"binary_value_name", REG_BINARY, data);
        //

        /**
          * \brief Writes a value under a specified key, the registry type based off the templated type passed as data
          * \tparam T The type capturing the data being set
          *         note - the type of registry value is determined by the template type T of data given
          * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
          * \param subkey A string to append to the HKEY to attempt to write
          *        can be nullptr if not needed
          * \param value_name A string specifying the name of the registry value to write
          *        can be nullptr to write to the unnamed default registry value
          * \param data The value containing the data to be set in the specified key
          * \exception std::exception (including wil::ResultException) will be thrown on all failures
          */
        template <typename T>
        void set_value(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const T& data)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.set_value(subkey, value_name, data);
        }

        /**
         * \brief Writes a value under a specified key, the registry type based off the templated type passed as data
         * \tparam T The type capturing the data being set
         *         note - the type of registry value is determined by the template type T of data given
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The value containing the data to be set in the specified key
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        template <typename T>
        void set_value(HKEY key, _In_opt_ PCWSTR value_name, const T& data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }

        /**
         * \brief Writes a REG_DWORD value from a uint32_t
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The 32-bit value to write to the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint32_t data)
        {
            return ::wil::reg::set_value(key, subkey, value_name, data);
        }

        /**
         * \brief Writes a REG_DWORD value from a uint32_t
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The 32-bit value to write to the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_dword(HKEY key, _In_opt_ PCWSTR value_name, uint32_t data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }

        /**
         * \brief Writes a REG_QWORD value from a uint64_t
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The 64-bit value to write to the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint64_t data)
        {
            return ::wil::reg::set_value(key, subkey, value_name, data);
        }

        /**
         * \brief Writes a REG_QWORD value from a uint64_t
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The 64-bit value to write to the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_qword(HKEY key, _In_opt_ PCWSTR value_name, uint64_t data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }

        /**
         * \brief Writes a REG_SZ value from a null-terminated string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The null-terminated string value to write to the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, PCWSTR data)
        {
            return ::wil::reg::set_value(key, subkey, value_name, data);
        }

        /**
         * \brief Writes a REG_SZ value from a null-terminated string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The null-terminated string value to write to the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_string(HKEY key, _In_opt_ PCWSTR value_name, PCWSTR data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }

        /**
         * \brief Writes a REG_EXPAND_SZ value from a null-terminated string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The null-terminated string value to write to the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, PCWSTR data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view regview{ key };
            return regview.set_value_with_type(subkey, value_name, data, REG_EXPAND_SZ);
        }

        /**
         * \brief Writes a REG_EXPAND_SZ value from a null-terminated string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The null-terminated string value to write to the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name, PCWSTR data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_expanded_string(key, nullptr, value_name, data);
        }

#if defined(_VECTOR_) && defined(_STRING_)
        /**
         * \brief The generic set_value template function to write a REG_MULTI_SZ value from a std::vector<std::wstring>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data A std::vector<std::wstring> to write to the specified registry value
         *        each string will be marshalled to a contiguous null-terminator-delimited multi-sz string
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        template <>
        inline void set_value(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.set_value(subkey, value_name, data);
        }

        /**
         * \brief The generic set_value template function to write a REG_MULTI_SZ value from a std::vector<std::wstring>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data A std::vector<std::wstring> to write to the specified registry value
         *        each string will be marshalled to a contiguous null-terminator-delimited multi-sz string
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        template <>
        inline void set_value(HKEY key, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }

        /**
         * \brief Writes a REG_MULTI_SZ value from a std::vector<std::wstring>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data A std::vector<std::wstring> to write to the specified registry value
         *        each string will be marshalled to a contiguous null-terminator-delimited multi-sz string
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_multistring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
        {
            return ::wil::reg::set_value(key, subkey, value_name, data);
        }

        /**
         * \brief Writes a REG_MULTI_SZ value from a std::vector<std::wstring>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data A std::vector<std::wstring> to write to the specified registry value
         *        each string will be marshalled to a contiguous null-terminator-delimited multi-sz string
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_multistring(HKEY key, _In_opt_ PCWSTR value_name, const ::std::vector<::std::wstring>& data)
        {
            return ::wil::reg::set_value(key, nullptr, value_name, data);
        }
#endif // #if defined(_VECTOR_) && defined(_STRING_)

#if defined(_VECTOR_)
        /**
         * \brief Writes a registry value of the specified type from a std::vector<BYTE>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param type The registry type for the specified registry value - see RegSetKeyValueW
         * \param data A std::vector<BYTE> to write to the specified registry value
         *        the vector contents will be directly marshalled to the specified value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_byte_vector(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type, const ::std::vector<BYTE>& data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view regview{ key };
            return regview.set_value_with_type(subkey, value_name, data, type);
        }

        /**
         * \brief Writes a registry value of the specified type from a std::vector<BYTE>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param type The registry type for the specified registry value - see RegSetKeyValueW
         * \param data A std::vector<BYTE> to write to the specified registry value
         *        the vector contents will be directly marshalled to the specified value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures
         */
        inline void set_value_byte_vector(HKEY key, _In_opt_ PCWSTR value_name, DWORD type, const ::std::vector<BYTE>& data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_byte_vector(key, nullptr, value_name, type, data);
        }
#endif // #if defined(_VECTOR_)
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

        //
        // template <typename T>
        // HRESULT set_value_nothrow(...)
        //
        //  - Writes a value under a specified key
        //  - The type of registry value is determined by the template type T of data given
        //  - Returns an HRESULT error code indicating success or failure (does not throw C++ exceptions)
        //
        // Examples of usage (the template type does not need to be explicitly specified)
        //     hr = wil::reg::set_value_nothrow(key, L"subkey", L"dword_value_name", 0); // writes a REG_DWORD
        //     hr = wil::reg::set_value_nothrow(key, L"subkey", L"qword_value_name", 0ull); // writes a REG_QWORD
        //     hr = wil::reg::set_value_nothrow(key, L"subkey", L"string_value_name", L"hello"); // writes a REG_SZ
        //
        // A subkey is not required if the key is opened where this should write the value:
        //     hr = wil::reg::set_value_nothrow(key, L"dword_value_name", 0); // writes a REG_DWORD
        //     hr = wil::reg::set_value_nothrow(key, L"qword_value_name", 0ull); // writes a REG_QWORD
        //     hr = wil::reg::set_value_nothrow(key, L"string_value_name", L"hello"); // writes a REG_SZ
        //
        // Example of usage writing a REG_MULTI_SZ
        //     std::vector<std::wstring> multisz_data { L"string1", L"string2", L"string3" };
        //     hr = wil::reg::set_value_nothrow(key, L"multi_string_value_name", multisz_data);
        //
        // Values can be written directly from a vector of bytes - the registry type must be specified; e.g.:
        //     std::vector<BYTE> data { 0x00, 0xff, 0xee, 0xdd, 0xcc };
        //     hr = wil::reg::set_value_byte_vector_nothrow(key, L"binary_value_name", REG_BINARY, data);
        //

        /**
          * \brief Writes a value under a specified key, the registry type based off the templated type passed as data
          * \tparam T The type capturing the data being set
          * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
          * \param subkey A string to append to the HKEY to attempt to write
          *        can be nullptr if not needed
          * \param value_name A string specifying the name of the registry value to write
          *        can be nullptr to write to the unnamed default registry value
          * \param data The value containing the data to be set in the specified key
          * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
          */
        template <typename T>
        HRESULT set_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, const T& data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.set_value(subkey, value_name, data);
        }

        /**
         * \brief Writes a value under a specified key, the registry type based off the templated type passed as data
         * \tparam T The type capturing the data being set
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
\         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The value containing the data to be set in the specified key
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template <typename T>
        HRESULT set_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, const T& data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }

        /**
         * \brief Writes a REG_DWORD value from a uint32_t
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The 32-bit value to write to the specified registry value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint32_t data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, subkey, value_name, data);
        }

        /**
         * \brief Writes a REG_DWORD value from a uint32_t
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The 32-bit value to write to the specified registry value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, uint32_t data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }

        /**
         * \brief Writes a REG_QWORD value from a uint64_t
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The 64-bit value to write to the specified registry value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, uint64_t data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, subkey, value_name, data);
        }

        /**
         * \brief Writes a REG_QWORD value from a uint64_t
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The 64-bit value to write to the specified registry value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, uint64_t data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }

        /**
         * \brief Writes a REG_SZ value from a null-terminated string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The null-terminated string value to write to the specified registry value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, PCWSTR data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, subkey, value_name, data);
        }

        /**
         * \brief Writes a REG_SZ value from a null-terminated string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The null-terminated string value to write to the specified registry value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, PCWSTR data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }

        /**
         * \brief Writes a REG_EXPAND_SZ value from a null-terminated string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The null-terminated string value to write to the specified registry value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, PCWSTR data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.set_value_with_type(subkey, value_name, data, REG_EXPAND_SZ);
        }

        /**
         * \brief Writes a REG_EXPAND_SZ value from a null-terminated string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data The null-terminated string value to write to the specified registry value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, PCWSTR data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_expanded_string_nothrow(key, nullptr, value_name, data);
        }

#if defined(WIL_ENABLE_EXCEPTIONS)
#if defined(_VECTOR_) && defined(_STRING_)
        /**
         * \brief Writes a REG_MULTI_SZ value from a std::vector<std::wstring>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data A std::vector<std::wstring> to write to the specified registry value
         *        each string will be marshalled to a contiguous null-terminator-delimited multi-sz string
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_multistring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, PCWSTR value_name, const ::std::vector<::std::wstring>& data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, subkey, value_name, data);
        }

        /**
         * \brief Writes a REG_MULTI_SZ value from a std::vector<std::wstring>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param data A std::vector<std::wstring> to write to the specified registry value
         *        each string will be marshalled to a contiguous null-terminator-delimited multi-sz string
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_multistring_nothrow(HKEY key, PCWSTR value_name, const ::std::vector<::std::wstring>& data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_nothrow(key, nullptr, value_name, data);
        }
#endif // #if defined(_VECTOR_) && defined(_STRING_)

#if defined(_VECTOR_)
        /**
         * \brief Writes a registry value of the specified type from a std::vector<BYTE>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to write
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param type The registry type for the specified registry value - see RegSetKeyValueW
         * \param data A std::vector<BYTE> to write to the specified registry value
         *        the vector contents will be directly marshalled to the specified value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type, const ::std::vector<BYTE>& data) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.set_value_with_type(subkey, value_name, data, type);
        }

        /**
         * \brief Writes a registry value of the specified type from a std::vector<BYTE>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to write
         *        can be nullptr to write to the unnamed default registry value
         * \param type The registry type for the specified registry value - see RegSetKeyValueW
         * \param data A std::vector<BYTE> to write to the specified registry value
         *        the vector contents will be directly marshalled to the specified value
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT set_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR value_name, DWORD type, const ::std::vector<BYTE>& data) WI_NOEXCEPT
        {
            return ::wil::reg::set_value_byte_vector_nothrow(key, nullptr, value_name, type, data);
        }
#endif // #if defined(_VECTOR_)
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

#if defined(WIL_ENABLE_EXCEPTIONS)
        //
        // template <typename T>
        // T get_value(...)
        //
        //  - Reads a value under a specified key, the required registry type based off the templated type passed as data
        //  - Throws a std::exception on failure (including wil::ResultException), including registry value not found
        //    if not wanting an exception when the value does not exist, use try_get_value(...)
        //
        // Examples of usage (ensure the code handles a possible std::exception that will be thrown on all errors)
        //     DWORD dword_value = wil::reg::get_value<DWORD>(key, L"subkey", L"dword_value_name");
        //     DWORD64 qword_value = wil::reg::get_value<DWORD64>(key, L"subkey", L"qword_value_name);
        //     std::wstring string_value = wil::reg::get_value<std::wstring>(key, L"subkey", L"string_value_name");
        //
        // A subkey is not required if the key is opened where this should write the value:
        //     DWORD dword_value = wil::reg::get_value<DWORD>(key, L"dword_value_name");
        //     DWORD64 qword_value = wil::reg::get_value<DWORD64>(key, L"qword_value_name);
        //     std::wstring string_value = wil::reg::get_value<std::wstring>(key, L"string_value_name");
        //
        // The template type does not need to be specified if using functions written for a targeted type
        //     DWORD dword_value = wil::reg::get_value_dword(key, L"dword_value_name");
        //     DWORD64 qword_value = wil::reg::get_value_qword(key, L"qword_value_name");
        //     std::wstring string_value = wil::reg::get_value_string(key, L"string_value_name");
        //
        // Values with REG_EXPAND_SZ can be read into each of the string types; e.g.:
        //     std::wstring expaned_string_value = wil::reg::get_value_expanded_string(key, L"string_value_name_with_environment_variables");
        //
        // Values can be read directly into a vector of bytes - the registry type must be specified; e.g.:
        //     std::vector<BYTE> data = wil::reg::get_value_byte_vector(key, L"binary_value_name", REG_BINARY);
        //
        // Multi-string values can be read into a vector<wstring>; e.g.:
        //     std::vector<std::wstring> multi_string_value = wil::reg::get_value_multistring(key, L"multi_string_value_name");
        //     for (const auto& sub_string_value : multi_string_value)
        //     {
        //         // can read each string parsed from the multi-string
        //         PCWSTR string_value = sub_string_value.c_str();
        //     }
        //
        // Reading REG_SZ and REG_EXPAND_SZ types are done through the below templated get_value_string and get_value_expanded_string functions
        // Where the template type is the type to receive the string value
        // The default template type is std::wstring, availble if the caller has included the STL <string> header
        //
        // Reading a bstr can be stored in a wil::shared_bstr or wil::unique_bstr - wil::shared_bstr has a c'tor taking a wil::unique_bstr
        //     wil::unique_bstr unique_value { wil::reg::get_value_string<::wil::unique_bstr>(key, L"string_value_name") };
        //     wil::shared_bstr shared_value { wil::reg::get_value_string<::wil::shared_bstr>(key, L"string_value_name") };
        //
        // Reading a cotaskmem string can be stored in a wil::unique_cotaskmem_string or wil::shared_cotaskmem_string
        //     wil::unique_cotaskmem_string unique_value { wil::reg::get_value_string<wil::unique_cotaskmem_string>(key, L"string_value_name") };
        //     wil::shared_cotaskmem_string shared_value { wil::reg::get_value_string<wil::shared_cotaskmem_string>(key, L"string_value_name") };
        //
        template <typename T>
        T get_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name);
        template <typename T>
        T get_value_string(HKEY key, _In_opt_ PCWSTR value_name);
        template <typename T>
        T get_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name);
        template <typename T>
        T get_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name);

        /**
         * \brief Reads a value under a specified key, the required registry type based off the templated type passed as data
         * \tparam T The type capturing the data being read (the type of registry value is determined by the template type T of data given)
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type T
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <typename T>
        T get_value(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            T return_value{};
            const reg_view_details::reg_view regview{ key };
            regview.get_value<T>(subkey, value_name, return_value);
            return return_value;
        }

        /**
         * \brief Reads a value under a specified key, the required registry type based off the templated type passed as data
         * \tparam T The type capturing the data being read
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type T
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <typename T>
        T get_value(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<T>(key, nullptr, value_name);
        }

        /**
         * \brief Reads a REG_DWORD value, returning a DWORD
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The DWORD value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline DWORD get_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<DWORD>(key, subkey, value_name);
        }

        /**
         * \brief Reads a REG_DWORD value, returning a DWORD
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The DWORD value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline DWORD get_value_dword(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<DWORD>(key, nullptr, value_name);
        }

        /**
         * \brief Reads a REG_QWORD value, returning a DWORD64
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The DWORD64 value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline DWORD64 get_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<DWORD64>(key, subkey, value_name);
        }

        /**
         * \brief Reads a REG_QWORD value, returning a DWORD64
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The DWORD64 value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline DWORD64 get_value_qword(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<DWORD64>(key, nullptr, value_name);
        }

#if defined(_STRING_)
        /**
         * \brief Reads a REG_SZ value, returning a std::wstring
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A std::wstring created from the string value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline ::std::wstring get_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::std::wstring>(key, subkey, value_name);
        }
        template <>
        inline ::std::wstring get_value_string<::std::wstring>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::std::wstring>(key, subkey, value_name);
        }

        /**
         * \brief Reads a REG_SZ value, returning a std::wstring
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A std::wstring created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline ::std::wstring get_value_string(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::std::wstring>(key, nullptr, value_name);
        }
        template <>
        inline ::std::wstring get_value_string<::std::wstring>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::std::wstring>(key, nullptr, value_name);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a std::wstring
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A std::wstring created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline ::std::wstring get_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            ::std::wstring value;
            const reg_view_details::reg_view regview{ key };
            regview.get_value(subkey, value_name, value, REG_EXPAND_SZ);
            return value;
        }
        template <>
        inline ::std::wstring get_value_expanded_string<::std::wstring>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value_expanded_string(key, subkey, value_name);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a std::wstring
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A std::wstring created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline ::std::wstring get_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value_expanded_string(key, nullptr, value_name);
        }
        template <>
        inline ::std::wstring get_value_expanded_string<::std::wstring>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value_expanded_string(key, nullptr, value_name);
        }
#endif // #if defined(_STRING_)

#if defined(__WIL_OLEAUTO_H_)
        /**
         * \brief Reads a REG_SZ value, returning a wil::unique_bstr
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::unique_bstr created from the string value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::unique_bstr get_value_string<::wil::unique_bstr>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::wil::unique_bstr>(key, subkey, value_name);
        }

        /**
         * \brief Reads a REG_SZ value, returning a wil::unique_bstr
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::unique_bstr created from the string value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::unique_bstr get_value_string<::wil::unique_bstr>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::wil::unique_bstr>(key, nullptr, value_name);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a wil::unqiue_bstr
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::unique_bstr created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::unique_bstr get_value_expanded_string<::wil::unique_bstr>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            ::wil::unique_bstr value;
            const reg_view_details::reg_view regview{ key };
            regview.get_value(subkey, value_name, value, REG_EXPAND_SZ);
            return value;
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a wil::unique_bstr
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::unique_bstr created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::unique_bstr get_value_expanded_string<::wil::unique_bstr>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value_expanded_string<::wil::unique_bstr>(key, nullptr, value_name);
        }
#if defined(__WIL_OLEAUTO_H_STL)
        /**
         * \brief Reads a REG_SZ value, returning a wil::shared_bstr
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::shared_bstr created from the string value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::shared_bstr get_value_string<::wil::shared_bstr>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::wil::shared_bstr>(key, subkey, value_name);
        }

        /**
         * \brief Reads a REG_SZ value, returning a wil::shared_bstr
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::shared_bstr created from the string value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::shared_bstr get_value_string<::wil::shared_bstr>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::wil::shared_bstr>(key, nullptr, value_name);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a wil::unqiue_bstr
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::shared_bstr created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::shared_bstr get_value_expanded_string<::wil::shared_bstr>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            ::wil::shared_bstr value;
            const reg_view_details::reg_view regview{ key };
            regview.get_value(subkey, value_name, value, REG_EXPAND_SZ);
            return value;
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a wil::shared_bstr
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::shared_bstr created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::shared_bstr get_value_expanded_string<::wil::shared_bstr>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value_expanded_string<::wil::shared_bstr>(key, nullptr, value_name);
        }
#endif // #if defined(__WIL_OLEAUTO_H_STL)
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
        /**
         * \brief Reads a REG_SZ value, returning a wil::unique_cotaskmem_string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::unique_cotaskmem_string created from the string value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::unique_cotaskmem_string get_value_string<::wil::unique_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::wil::unique_cotaskmem_string>(key, subkey, value_name);
        }

        /**
         * \brief Reads a REG_SZ value, returning a wil::unique_cotaskmem_string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::unique_cotaskmem_string created from the string value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::unique_cotaskmem_string get_value_string<::wil::unique_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::wil::unique_cotaskmem_string>(key, nullptr, value_name);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a wil::unique_cotaskmem_string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::unique_cotaskmem_string created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::unique_cotaskmem_string get_value_expanded_string<::wil::unique_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            ::wil::unique_cotaskmem_string value;
            const reg_view_details::reg_view regview{ key };
            regview.get_value(subkey, value_name, value, REG_EXPAND_SZ);
            return value;
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a wil::unique_cotaskmem_string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::unique_cotaskmem_string created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::unique_cotaskmem_string get_value_expanded_string<::wil::unique_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value_expanded_string<::wil::unique_cotaskmem_string>(key, nullptr, value_name);
        }
#if defined(__WIL_OBJBASE_H_STL)
        /**
         * \brief Reads a REG_SZ value, returning a wil::shared_cotaskmem_string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::shared_cotaskmem_string created from the string value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::shared_cotaskmem_string get_value_string<::wil::shared_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::wil::shared_cotaskmem_string>(key, subkey, value_name);
        }

        /**
         * \brief Reads a REG_SZ value, returning a wil::shared_cotaskmem_string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::shared_cotaskmem_string created from the string value read from the registry
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::shared_cotaskmem_string get_value_string<::wil::shared_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value<::wil::shared_cotaskmem_string>(key, nullptr, value_name);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a wil::shared_cotaskmem_string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::shared_cotaskmem_string created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::shared_cotaskmem_string get_value_expanded_string<::wil::shared_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            ::wil::shared_cotaskmem_string value;
            const reg_view_details::reg_view regview{ key };
            regview.get_value(subkey, value_name, value, REG_EXPAND_SZ);
            return value;
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value, returning a wil::shared_cotaskmem_string
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return A wil::shared_cotaskmem_string created from the string value read from the registry
         *         note, the returned string will have already passed through ExpandEnvironmentStringsW
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        template <>
        inline ::wil::shared_cotaskmem_string get_value_expanded_string<::wil::shared_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return wil::reg::get_value_expanded_string<::wil::shared_cotaskmem_string>(key, nullptr, value_name);
        }
#endif // #if defined(__WIL_OBJBASE_H_STL)
#endif // defined(__WIL_OBJBASE_H_)

#if defined(_VECTOR_)
        /**
         * \brief Reads a registry value of the specified type, returning a std::vector<BYTE>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param type The registry type for the specified registry value to read from - see RegGetValueW
         * \return A std::vector<BYTE> containing the bytes of the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline ::std::vector<BYTE> get_value_byte_vector(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type)
        {
            ::std::vector<BYTE> return_value{};
            const reg_view_details::reg_view regview{ key };
            regview.get_value(subkey, value_name, return_value, type);
            return return_value;
        }

        /**
         * \brief Reads a registry value of the specified type, returning a std::vector<BYTE>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param type The registry type for the specified registry value to read from - see RegGetValueW
         * \return A std::vector<BYTE> containing the bytes of the specified registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline ::std::vector<BYTE> get_value_byte_vector(HKEY key, _In_opt_ PCWSTR value_name, DWORD type)
        {
            return ::wil::reg::get_value_byte_vector(key, nullptr, value_name, type);
        }
#endif // #if defined(_VECTOR_)

#if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        /**
         * \brief Reads a REG_MULTI_SZ value, returning a std::vector<std::wstring>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The std::vector<std::wstring> marshalled from data read from the registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         *
         * \remark Note that will return empty strings for embedded nulls - it won't stop at the first double-null character
         *         e.g. a REG_MULTI_SZ of L"string1\0\0string2\0\0string3\0\0"
         *              returns a vector of size 5: L"string1", empty-string, L"string2", empty-string, L"string3"
         */
        inline ::std::vector<::std::wstring> get_value_multistring(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            ::std::vector<::std::wstring> return_value;
            ::std::vector<BYTE> rawData{ get_value_byte_vector(key, subkey, value_name, REG_MULTI_SZ) };
            if (!rawData.empty())
            {
                auto* const begin = reinterpret_cast<wchar_t*>(rawData.data());
                auto* const end = begin + rawData.size() / sizeof(wchar_t);
                return_value = ::wil::reg::reg_view_details::get_wstring_vector_from_multistring(::std::vector<wchar_t>(begin, end));
            }

            return return_value;
        }

        /**
         * \brief Reads a REG_MULTI_SZ value, returning a std::vector<std::wstring>
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The std::vector<std::wstring> marshalled from data read from the registry value
         * \exception std::exception (including wil::ResultException) will be thrown on all failures, including value not found
         */
        inline ::std::vector<::std::wstring> get_value_multistring(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::get_value_multistring(key, nullptr, value_name);
        }

#endif // #if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined (_OPTIONAL_) && defined(__cpp_lib_optional)
        //
        // template <typename T>
        // void try_get_value(...)
        //
        //  - Reads a value under a specified key, the required registry type based off the templated type passed as data
        //  - throws a std::exception on failure (including wil::ResultException), except if the registry value was not found
        //    returns a std::nullopt if the registry value is not found
        //
        // Examples using the returned std::optional<DWORD>
        //  - Caller should ensure the code handles a possible std::exception that will be thrown on all errors except value not found
        //
        //     std::optional<DWORD> opt_dword_value = wil::reg::try_get_value<DWORD>(key, L"dword_value_name");
        //     if (opt_dword_value.has_value())
        //     {
        //         // opt_dword_value.value() returns the DWORD read from the registry
        //     }
        //     else
        //     {
        //         // the registry value did not exist
        //     }
        //     // if the caller wants to apply a default value of 0, they can call value_or()
        //     DWORD opt_dword_value = wil::reg::try_get_value<DWORD>(key, L"dword_value_name").value_or(0);
        //
        // Examples using the returned std::optional<std::wstring>
        //     std::optional<std::wstring> opt_string_value = wil::reg::try_get_value_string(key, L"string_value_name");
        //     if (opt_string_value.has_value())
        //     {
        //         // opt_string_value.value() returns the std::wstring read from the registry
        //         // the below avoids copying the std::wstring as value() here returns a std::wstring&
        //         PCWSTR string_value = opt_string_value.value().c_str();
        //     }
        //     else
        //     {
        //         // the registry value did not exist
        //     }
        //
        //     // if the caller wants to apply a default value of L"default", they can call value_or()
        //     // note that std::optional only attempts to construct a std::wstring for L"default" if the std::optional is empty (std::nullopt)
        //     // thus only allocating a new std::wsting for the default value when it's needed
        //     std::optional<std::wstring> opt_string_value = wil::reg::try_get_value_string(key, L"string_value_name").value_or(L"default");
        //
        // Examples of usage:
        //     std::optional<DWORD> opt_dword_value = wil::reg::try_get_value<DWORD>(key, L"subkey", L"dword_value_name");
        //     std::optional<DWORD64> opt_qword_value = wil::reg::try_get_value<DWORD64>(key, L"subkey", L"qword_value_name);
        //     std::optional<std::wstring> opt_string_value = wil::reg::try_get_value<std::wstring>(key, L"subkey", L"string_value_name");
        //
        // A subkey is not required if the key is opened where this should write the value; e.g.
        //     std::optional<DWORD> opt_dword_value = wil::reg::try_get_value<DWORD>(key, L"dword_value_name");
        //     std::optional<DWORD64> opt_qword_value = wil::reg::try_get_value<DWORD64>(key, L"qword_value_name);
        //     std::optional<std::wstring> opt_string_value = wil::reg::try_get_value<std::wstring>(key, L"string_value_name");
        //
        // The template type does not need to be specified if using functions written for a targeted type; e.g.
        //     std::optional<DWORD> opt_dword_value = wil::reg::try_get_value_dword(key, L"dword_value_name");
        //     std::optional<DWORD64> opt_qword_value = wil::reg::try_get_value_qword(key, L"qword_value_name");
        //     std::optional<std::wstring> opt_string_value = wil::reg::try_get_value_string(key, L"string_value_name");
        //
        // Values with REG_EXPAND_SZ can be read into each of the string types; e.g.:
        //     std::optional<std::wstring> opt_expaned_string_value = wil::reg::try_get_value_expanded_string(key, L"string_value_name_with_environment_variables");
        //
        // Values can be read directly into a vector of bytes - the registry type must be specified; e.g.:
        //     std::optional<std::vector<BYTE>> opt_data = wil::reg::try_get_value_byte_vector(key, L"binary_value_name", REG_BINARY);
        //
        // Multi-string values can be read into a std::vector<std::wstring>; e.g.:
        //     std::optional<::std::vector<::std::wstring>> try_get_value_multistring(key, L"multi_string_value_name");
        //     See the definition of try_get_value_multistring before for usage guidance
        //
        // Reading REG_SZ and REG_EXPAND_SZ types are done through the below templated try_get_value_string and try_get_value_expanded_string functions
        // Where the template type is the type to receive the string value
        // The default template type is std::wstring, availble if the caller has included the STL <string> header
        //
        // Reading a bstr is returned in a std::optional<wil::shared_bstr> - because wil::unique_bstr cannot be copied and thus is difficult to work with a std::optional
        //     std::optional<wil::shared_bstr> shared_value { wil::reg::try_get_value_string<::wil::shared_bstr>(key, L"string_value_name") };
        //
        // Reading a cotaskmem string is returned in a std::optional<wil::shared_cotaskmem_string> - because wil::unique_cotaskmem_string cannot be copied and thus is difficult to work with a std::optional
        //     std::optional<wil::shared_cotaskmem_string> opt_shared_value { wil::reg::try_get_value_string<wil::shared_cotaskmem_string>(key, L"string_value_name") };
        //
        template <typename T>
        ::std::optional<T> try_get_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name);
        template <typename T>
        ::std::optional<T> try_get_value_string(HKEY key, _In_opt_ PCWSTR value_name);
        template <typename T>
        ::std::optional<T> try_get_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name);
        template <typename T>
        ::std::optional<T> try_get_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name);

        /**
         * \brief Attempts to read a value under a specified key, returning in a std::optional, the required registry type based off the templated return type
         * \tparam T The type capturing the data being read into a std::optional
         *         the type of registry value is determined by the template type T of data given
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<T>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <typename T>
        ::std::optional<T> try_get_value(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<T>(subkey, value_name);
        }

        /**
         * \brief Attempts to read a value under a specified key, returning the value in a std::optional, the required registry type based off the templated return type
         * \tparam T The type capturing the data being read into a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<T>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <typename T>
        ::std::optional<T> try_get_value(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value<T>(key, nullptr, value_name);
        }

        /**
         * \brief Attempts to read a REG_DWORD value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<DWORD>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<DWORD> try_get_value_dword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value<DWORD>(key, subkey, value_name);
        }

        /**
         * \brief Attempts to read a REGD_DWORD value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<DWORD>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<DWORD> try_get_value_dword(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value<DWORD>(key, nullptr, value_name);
        }

        /**
         * \brief Attempts to read a REG_QWORD value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<DWORD64>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<DWORD64> try_get_value_qword(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value<DWORD64>(key, subkey, value_name);
        }

        /**
         * \brief Attempts to read a REG_QWORD value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value stored in a std::optional<DWORD64>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<DWORD64> try_get_value_qword(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value<DWORD64>(key, nullptr, value_name);
        }

#if defined(_VECTOR_)
        /**
         * \brief Attempts to read a value under a specified key requiring the specified type, returning the raw bytes in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param type The registry type for the specified registry value to read from - see RegGetValueW
         * \return The raw bytes read from the registry value stored in a std::optional<std::vector<BYTE>>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<::std::vector<BYTE>> try_get_value_byte_vector(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::std::vector<BYTE>>(subkey, value_name, type);
        }

        /**
         * \brief Attempts to read a value under a specified key requiring the specified type, returning the raw bytes in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param type The registry type for the specified registry value to read from - see RegGetValueW
         * \return The raw bytes read from the registry value stored in a std::optional<std::vector<BYTE>>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<::std::vector<BYTE>> try_get_value_byte_vector(HKEY key, _In_opt_ PCWSTR value_name, DWORD type)
        {
            return ::wil::reg::try_get_value_byte_vector(key, nullptr, value_name, type);
        }
#endif // #if defined(_VECTOR_)


#if defined(_STRING_)
        /**
         * \brief Attempts to read a REG_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<std::wstring>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<::std::wstring> try_get_value_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::std::wstring>(subkey, value_name);
        }
        template <>
        inline ::std::optional<::std::wstring> try_get_value_string<::std::wstring>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_string(key, subkey, value_name);
        }

        /**
         * \brief Attempts to read a REG_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<std::wstring>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<::std::wstring> try_get_value_string(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_string(key, nullptr, value_name);
        }
        template <>
        inline ::std::optional<::std::wstring> try_get_value_string<::std::wstring>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_string(key, nullptr, value_name);
        }

        /**
         * \brief Attempts to read a REG_EXPAND_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<std::wstring>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<::std::wstring> try_get_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::std::wstring>(subkey, value_name, REG_EXPAND_SZ);
        }
        template <>
        inline ::std::optional<::std::wstring> try_get_value_expanded_string<::std::wstring>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_expanded_string(key, subkey, value_name);
        }

        /**
         * \brief Attempts to read a REG_EXPAND_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<std::wstring>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<::std::wstring> try_get_value_expanded_string(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_expanded_string(key, nullptr, value_name);
        }
        template <>
        inline ::std::optional<::std::wstring> try_get_value_expanded_string<::std::wstring>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_expanded_string(key, nullptr, value_name);
        }
#endif // #if defined(_STRING_)

#if defined(__WIL_OLEAUTO_H_STL)
        /**
         * \brief Attempts to read a REG_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<wil::shared_bstr>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <>
        inline ::std::optional<::wil::shared_bstr> try_get_value_string<::wil::shared_bstr>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::wil::shared_bstr>(subkey, value_name);
        }

        /**
         * \brief Attempts to read a REG_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<wil::shared_bstr>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <>
        inline ::std::optional<::wil::shared_bstr> try_get_value_string<::wil::shared_bstr>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_string<::wil::shared_bstr>(key, nullptr, value_name);
        }

        /**
         * \brief Attempts to read a REG_EXPAND_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<wil::shared_bstr>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <>
        inline ::std::optional<::wil::shared_bstr> try_get_value_expanded_string<::wil::shared_bstr>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::wil::shared_bstr>(subkey, value_name, REG_EXPAND_SZ);
        }

        /**
         * \brief Attempts to read a REG_EXPAND_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<wil::shared_bstr>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <>
        inline ::std::optional<::wil::shared_bstr> try_get_value_expanded_string<::wil::shared_bstr>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_expanded_string<::wil::shared_bstr>(key, nullptr, value_name);
        }
#endif // #if defined(__WIL_OLEAUTO_H_STL)

#if defined(__WIL_OBJBASE_H_STL)
        /**
         * \brief Attempts to read a REG_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<wil::shared_cotaskmem_string>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <>
        inline ::std::optional<::wil::shared_cotaskmem_string> try_get_value_string<::wil::shared_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::wil::shared_cotaskmem_string>(subkey, value_name);
        }

        /**
         * \brief Attempts to read a REG_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<wil::shared_cotaskmem_string>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <>
        inline ::std::optional<::wil::shared_cotaskmem_string> try_get_value_string<::wil::shared_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_string<::wil::shared_cotaskmem_string>(key, nullptr, value_name);
        }

        /**
         * \brief Attempts to read a REG_EXPAND_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<:wil::shared_cotaskmem_string>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <>
        inline ::std::optional<::wil::shared_cotaskmem_string> try_get_value_expanded_string(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name)
        {
            const reg_view_details::reg_view regview{ key };
            return regview.try_get_value<::wil::shared_cotaskmem_string>(subkey, value_name, REG_EXPAND_SZ);
        }

        /**
         * \brief Attempts to read a REG_EXPAND_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<wil::shared_cotaskmem_string>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        template <>
        inline ::std::optional<::wil::shared_cotaskmem_string> try_get_value_expanded_string<::wil::shared_cotaskmem_string>(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_expanded_string<::wil::shared_cotaskmem_string>(key, nullptr, value_name);
        }
#endif // defined(__WIL_OBJBASE_H_STL)
#endif // #if defined (_OPTIONAL_) && defined(__cpp_lib_optional)
#endif // #if defined(WIL_ENABLE_EXCEPTIONS)

        //
        // template <typename T>
        // HRESULT get_value_nothrow(...)
        //
        //  - Reads a value from under a specified key
        //  - The required type of registry value being read from is determined by the template type T
        //  - Returns an HRESULT error code indicating success or failure (does not throw C++ exceptions)
        //
        // Examples of usage (the template type does not need to be explicitly specified)
        //     DWORD dword_value{};
        //     hr = wil::reg::get_value_nothrow(key, L"subkey", L"dword_value_name", &dword_value); // reads a REG_DWORD
        //     DWORD6 qword_value{};
        //     hr = wil::reg::get_value_nothrow(key, L"subkey", L"qword_value_name", &qword_value); // reads a REG_QWORD
        //     std::wstring string_value{};
        //     hr = wil::reg::get_value_nothrow(key, L"subkey", L"string_value_name", &string_value); // reads a REG_SZ
        //
        // A subkey is not required if the key is opened where this should write the value:
        //     hr = wil::reg::get_value_nothrow(key, L"dword_value_name", &dword_value);
        //     hr = wil::reg::get_value_nothrow(key, L"qword_value_name", &qword_value); // reads a REG_QWORD
        //     hr = wil::reg::get_value_nothrow(key, L"string_value_name", &string_value);
        //
        // Can also specify the registry type in the function name:
        //     hr = wil::reg::get_value_dword_nothrow(key, L"dword_value_name", &dword_value);
        //     hr = wil::reg::get_value_qword_nothrow(key, L"qword_value_name", &qword_value); // reads a REG_QWORD
        //     hr = wil::reg::get_value_string_nothrow(key, L"string_value_name", &string_value);
        //
        // Example storing directly intto a WCHAR array - note will return the required number of bytes if the supplied array is too small
        //     WCHAR string_value[100]{};
        //     DWORD requiredBytes{};
        //     hr = wil::reg::get_value_string_nothrow(key, L"string_value_name", string_value, &requiredBytes);
        //
        // Example of usage writing a REG_MULTI_SZ
        //     std::vector<std::wstring> string_values{};
        //     hr = wil::reg::get_value_multistring_nothrow(key, L"multi_string_value_name", &string_values);
        //
        // Values can be written directly from a vector of bytes - the registry type must be specified; e.g.:
        //     std::vector<BYTE> raw_value{};
        //     hr = wil::reg::get_value_byte_vector_nothrow(key, L"binary_value_name", REG_BINARY, &raw_value);
        //
        //
        // Reading REG_SZ and REG_EXPAND_SZ types are done through the below templated get_value_string_nothrow and get_value_expaneded_string_nothrow functions
        // Where the template type is the type to receive the string value
        // The default template type is std::wstring, availble if the caller has included the STL <string> header
        //
        // Example storing a string in a wil::unique_bstr, wil::shared_bstr, wil::unique_cotaskmem_string, or wil::shared_cotaskmem_string
        /// - These string types are passed by reference, not by pointer, because the wil types overload the & operator
        //
        //     wil::unique_bstr bstr_value{};
        //     hr = wil::reg::get_value_nothrow(key, L"string_value_name", bstr_value);
        //     // or can specify explicity reading a string into a wil::unique_bstr type
        //     hr = wil::reg::get_value_string_nothrow(key, L"string_value_name", bstr_value);
        //
        //     wil::shared_bstr shared_bstr_value{};
        //     hr = wil::reg::get_value_nothrow(key, L"string_value_name", shared_bstr_value);
        //     // or can specify explicity reading a string into a wil::shared_bstr type
        //     hr = wil::reg::get_value_string_nothrow(key, L"string_value_name", shared_bstr_value);
        //
        //     wil::unique_cotaskmem_string string_value{};
        //     hr = wil::reg::get_value_nothrow(key, L"string_value_name", string_value);
        //     // or can specify explicity reading a string into a wil::unique_cotaskmem_string type
        //     hr = wil::reg::get_value_string_nothrow(key, L"string_value_name", string_value);
        //
        //     wil::shared_cotaskmem_string shared_string_value{};
        //     hr = wil::reg::get_value_nothrow(key, L"string_value_name", shared_string_value);
        //     // or can specify explicity reading a string into a wil::shared_cotaskmem_string type
        //     hr = wil::reg::get_value_string_nothrow(key, L"string_value_name", shared_string_value);
        //

        /**
         * \brief Reads a value under a specified key, the registry type based off the templated type passed as data
         * \tparam T The type capturing the data being set
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A pointer-to-T receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template<typename T,
            std::enable_if_t<!std::is_same_v<T, wchar_t>>* = nullptr>
        HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Out_ T* return_value) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value<T>(subkey, value_name, *return_value);
        }

        /**
         * \brief Reads a value under a specified key, the registry type based off the templated type passed as data
         * \tparam T The type capturing the data being set
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A pointer-to-T receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template<typename T,
            std::enable_if_t<!std::is_same_v<T, wchar_t>>* = nullptr>
        HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Out_ T* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \tparam Length The length of the WCHAR array passed as an OUT parameter
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A WCHAR array receiving the value read from the registry
         *             will write to the WCHAR array the string value read from the registry, guaranteeing null-termination
         * \param[out] requiredBytes An optional pointer to a DWORD to receive the required bytes of the string in the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template <size_t Length>
        HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* requiredBytes = nullptr) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value_char_array(subkey, value_name, return_value, REG_SZ, requiredBytes);
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \tparam Length The length of the WCHAR array passed as an OUT parameter
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A WCHAR array receiving the value read from the registry
         *             will write to the WCHAR array the string value read from the registry, guaranteeing null-termination
         * \param[out] requiredBytes An optional pointer to a DWORD to receive the required bytes of the string to be read
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template <size_t Length>
        HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* requiredBytes = nullptr) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_string_nothrow<Length>(key, nullptr, value_name, return_value, requiredBytes);
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \tparam Length The length of the WCHAR array passed as an OUT parameter
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A WCHAR array receiving the value read from the registry
         *             will write to the WCHAR array the string value read from the registry, guaranteeing null-termination
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template <size_t Length>
        HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length]) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_string_nothrow<Length>(key, subkey, value_name, return_value, nullptr);
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \tparam Length The length of the WCHAR array passed as an OUT parameter
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A WCHAR array receiving the value read from the registry
         *             will write to the WCHAR array the string value read from the registry, guaranteeing null-termination
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template <size_t Length>
        HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length]) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_string_nothrow<Length>(key, nullptr, value_name, return_value, nullptr);
        }

        /**
         * \brief Reads a REG_DWORD value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A DWORD receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Out_ DWORD* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value);
        }

        /**
         * \brief Reads a REG_DWORD value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A DWORD receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_dword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Out_ DWORD* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }

        /**
         * \brief Reads a REG_QWORD value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A DWORD64 receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Out_ DWORD64* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value);
        }

        /**
         * \brief Reads a REG_QWORD value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A DWORD64 receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_qword_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Out_ DWORD64* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A std::wstring receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::std::wstring& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, &return_value);
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A std::wstring receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::std::wstring& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, &return_value);
        }
#endif

#if defined(__WIL_OLEAUTO_H_)
        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::unique_bstr receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::wil::unique_bstr& return_value) WI_NOEXCEPT
        {
            return_value.reset();
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value.addressof());
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::unique_bstr receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::wil::unique_bstr& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_string_nothrow(key, nullptr, value_name, return_value);
        }

#if defined(__WIL_OLEAUTO_H_STL)
        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::shared_bstr receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::wil::shared_bstr& return_value) WI_NOEXCEPT
        {
            return_value.reset();
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value.addressof());
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::shared_bstr receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::wil::shared_bstr& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_string_nothrow(key, nullptr, value_name, return_value);
        }
#endif // #if defined(__WIL_OLEAUTO_H_STL)
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::unique_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, ::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return_value.reset();
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value(subkey, value_name, return_value);
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::unique_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, ::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }

        /**
          * \brief Reads a REG_SZ value under a specified key
          * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
          * \param subkey A string to append to the HKEY to attempt to read from
          *        can be nullptr if not needed
          * \param value_name A string specifying the name of the registry value to read from
          *        can be nullptr to read from the unnamed default registry value
          * \param[out] return_value A wil::unique_cotaskmem_string receiving the value read from the registry
          * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
          */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value);
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::unique_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }
#if defined(__WIL_OBJBASE_H_STL)
        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::shared_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, ::wil::shared_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return_value.reset();
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value(subkey, value_name, return_value);
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::shared_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, ::wil::shared_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }

        /**
          * \brief Reads a REG_SZ value under a specified key
          * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
          * \param subkey A string to append to the HKEY to attempt to read from
          *        can be nullptr if not needed
          * \param value_name A string specifying the name of the registry value to read from
          *        can be nullptr to read from the unnamed default registry value
          * \param[out] return_value A wil::shared_cotaskmem_string receiving the value read from the registry
          * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
          */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::wil::shared_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, subkey, value_name, return_value);
        }

        /**
         * \brief Reads a REG_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::shared_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::wil::shared_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_nothrow(key, nullptr, value_name, return_value);
        }
#endif // #if defined(__WIL_OBJBASE_H_STL)
#endif // defined(__WIL_OBJBASE_H_)

#if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)
        /**
         * \brief Reads the raw bytes from a registry value under a specified key of the specified type
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param type The registry type for the specified registry value to read from - see RegGetValueW
         * \param[out] return_value A std::vector<BYTE> receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, DWORD type, _Inout_::std::vector<BYTE>* return_value) WI_NOEXCEPT try
        {
            // zero the vector if it already had a buffer
            for (auto& byte_value : *return_value)
            {
                byte_value = 0x00;
            }
            const reg_view_details::reg_view_nothrow regview{ key };
            RETURN_IF_FAILED(regview.get_value<::std::vector<BYTE>>(subkey, value_name, *return_value, type));
            return S_OK;
        }
        CATCH_RETURN();

        /**
         * \brief Reads the raw bytes from a registry value under a specified key of the specified type
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param type The registry type for the specified registry value to read from - see RegGetValueW
         * \param[out] return_value A std::vector<BYTE> receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_byte_vector_nothrow(HKEY key, _In_opt_ PCWSTR value_name, DWORD type, _Inout_::std::vector<BYTE>* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_byte_vector_nothrow(key, nullptr, value_name, type, return_value);
        }
#endif // #if defined(_VECTOR_) && defined(WIL_ENABLE_EXCEPTIONS)

        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \tparam Length The length of the WCHAR array passed as an OUT parameter
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A WCHAR array receiving the value read from the registry
         *             will write to the WCHAR array the string value read from the registry, guaranteeing null-termination
         * \param[out] requiredBytes An optional pointer to a DWORD to receive the required bytes of the string to be read
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template <size_t Length>
        HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* requiredBytes = nullptr) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value_char_array(subkey, value_name, return_value, REG_EXPAND_SZ, requiredBytes);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \tparam Length The length of the WCHAR array passed as an OUT parameter
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A WCHAR array receiving the value read from the registry
         *             will write to the WCHAR array the string value read from the registry, guaranteeing null-termination
         * \param[out] requiredBytes An optional pointer to a DWORD to receive the required bytes of the string to be read
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template <size_t Length>
        HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, WCHAR(&return_value)[Length], _Out_opt_ DWORD* requiredBytes = nullptr) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_expanded_string_nothrow<Length>(key, nullptr, value_name, return_value, requiredBytes);
        }

#if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A std::wstring receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::std::wstring& return_value) WI_NOEXCEPT try
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            RETURN_IF_FAILED(regview.get_value<::std::wstring>(subkey, value_name, return_value, REG_EXPAND_SZ));
            return S_OK;
        }
        CATCH_RETURN();

        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A std::wstring receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::std::wstring& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_expanded_string_nothrow(key, nullptr, value_name, return_value);
        }
#endif // #if defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)

#if defined(__WIL_OLEAUTO_H_)
        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::unique_bstr receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::wil::unique_bstr& return_value) WI_NOEXCEPT
        {
            return_value.reset();
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value<::wil::unique_bstr>(subkey, value_name, return_value, REG_EXPAND_SZ);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::unique_bstr receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::wil::unique_bstr& return_value) WI_NOEXCEPT
        {
            return wil::reg::get_value_expanded_string_nothrow(key, nullptr, value_name, return_value);
        }

#if defined(__WIL_OLEAUTO_H_STL)
        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::shared_bstr receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::wil::shared_bstr& return_value) WI_NOEXCEPT
        {
            return_value.reset();
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value<::wil::shared_bstr>(subkey, value_name, return_value, REG_EXPAND_SZ);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::shared_bstr receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::wil::shared_bstr& return_value) WI_NOEXCEPT try
        {
            return ::wil::reg::get_value_expanded_string_nothrow(key, nullptr, value_name, return_value);
        }
        CATCH_RETURN();
#endif // #if defined(__WIL_OLEAUTO_H_STL)
#endif // #if defined(__WIL_OLEAUTO_H_)

#if defined(__WIL_OBJBASE_H_)
        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::unique_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value<::wil::unique_cotaskmem_string>(subkey, value_name, return_value, REG_EXPAND_SZ);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::unique_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::wil::unique_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_expanded_string_nothrow(key, nullptr, value_name, return_value);
        }
#if defined(__WIL_OBJBASE_H_STL)
        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::shared_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::wil::shared_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            const reg_view_details::reg_view_nothrow regview{ key };
            return regview.get_value<::wil::shared_cotaskmem_string>(subkey, value_name, return_value, REG_EXPAND_SZ);
        }

        /**
         * \brief Reads a REG_EXPAND_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A wil::shared_cotaskmem_string receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_expanded_string_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::wil::shared_cotaskmem_string& return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_expanded_string_nothrow(key, nullptr, value_name, return_value);
        }
#endif // #if defined(__WIL_OBJBASE_H_STL)
#endif // defined(__WIL_OBJBASE_H_)

#if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
        /**
         * \brief Reads a REG_MULTI_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A std::vector<std::wstring> receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_multistring_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT try
        {
            return_value->clear();

            ::std::vector<BYTE> rawData;
            RETURN_IF_FAILED(::wil::reg::get_value_byte_vector_nothrow(key, subkey, value_name, REG_MULTI_SZ, &rawData));
            if (!rawData.empty())
            {
                auto* const begin = reinterpret_cast<wchar_t*>(rawData.data());
                auto* const end = begin + rawData.size() / sizeof(wchar_t);
                *return_value = ::wil::reg::reg_view_details::get_wstring_vector_from_multistring(::std::vector<wchar_t>(begin, end));
            }
            return S_OK;
        }
        CATCH_RETURN();

        /**
         * \brief Reads a REG_MULTI_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A std::vector<std::wstring> receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        inline HRESULT get_value_multistring_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_multistring_nothrow(key, nullptr, value_name, return_value);
        }

        /**
         * \brief Reads a REG_MULTI_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A std::vector<std::wstring> receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template<>
        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR subkey, _In_opt_ PCWSTR value_name, _Inout_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_multistring_nothrow(key, subkey, value_name, return_value);
        }

        /**
         * \brief Reads a REG_MULTI_SZ value under a specified key
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \param[out] return_value A std::vector<std::wstring> receiving the value read from the registry
         * \return HRESULT error code indicating success or failure (does not throw C++ exceptions)
         */
        template<>
        inline HRESULT get_value_nothrow(HKEY key, _In_opt_ PCWSTR value_name, _Inout_::std::vector<::std::wstring>* return_value) WI_NOEXCEPT
        {
            return ::wil::reg::get_value_multistring_nothrow(key, nullptr, value_name, return_value);
        }

#if defined (_OPTIONAL_) && defined(__cpp_lib_optional)
        /**
         * \brief Attempts to read a REG_MULTI_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param subkey A string to append to the HKEY to attempt to read from
         *        can be nullptr if not needed
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value of the template type std::optional<std::vector<std::wstring>>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         *
         * \remark Example usage:
         *         // ensure the code handles a possible std::exception that will be thrown on all errors
         *
         *         std::optional<std::vector<std::wstring>> opt_vector_value = wil::reg::try_get_value_multistring(key, L"subkey", L"value_name");
         *         if (opt_vector_value.has_value())
         *         {
         *             // note this won't make any copies of the vector, nor any wstring - as value() returns a std::vector<std::wstring>&
         *             for (const auto& sub_string_value : opt_string_value.value())
         *             {
         *                 // can read each string parsed from the multi-string
         *                 PCWSTR string_value = sub_string_value.c_str();
         *             }
         *         }
         *         else
         *         {
         *             // the value does not exist
         *         }
         *
         *         // if the caller wants to apply a default value of an empty vector, they can call value_or()
         *         std::optional<std::vector<std::wstring>> opt_vector_value =
         *             wil::reg::try_get_value_multistring(key, L"subkey", L"value_name").value_or(std::vector<std::wstring>{});
         *
         *         // ensure the code handles a possible std::exception that could be thrown above
         *         // in this example, returns the HRESULT error code if anything fails reading the registry value other than value not found
         */
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

        /**
         * \brief Attempts to read a REG_MULTI_SZ value under a specified key, returning the value in a std::optional
         * \param key An opened registry key, or fixed registry key as the base key, from which to append the path
         * \param value_name A string specifying the name of the registry value to read from
         *        can be nullptr to read from the unnamed default registry value
         * \return The value read from the registry value marshalled to a std::optional<std::vector<std::wstring>>
         *         note - will return std::nullopt if the value does not exist
         * \exception std::exception (including wil::ResultException) will be thrown on failures except value not found
         */
        inline ::std::optional<::std::vector<::std::wstring>> try_get_value_multistring(HKEY key, _In_opt_ PCWSTR value_name)
        {
            return ::wil::reg::try_get_value_multistring(key, nullptr, value_name);
        }
#endif // #if defined (_OPTIONAL_) && defined(__cpp_lib_optional)
#endif // #if defined(_VECTOR_) && defined(_STRING_) && defined(WIL_ENABLE_EXCEPTIONS)
    }

    // unique_registry_watcher/unique_registry_watcher_nothrow/unique_registry_watcher_failfast
    // These classes make it easy to execute a provided function when a
    // registry key changes (optionally recursively). Specify the key
    // either as a root key + path, or an open registry handle as wil::unique_hkey
    // or a raw HKEY value (that will be duplicated).
    //
    // Example use with exceptions base error handling:
    // auto watcher = wil::make_registry_watcher(HKEY_CURRENT_USER, L"Software\\MyApp", true, wil::RegistryChangeKind changeKind[]
    //     {
    //          if (changeKind == RegistryChangeKind::Delete)
    //          {
    //              watcher.reset();
    //          }
    //         // invalidate cached registry data here
    //     });
    //
    // Example use with error code base error handling:
    // auto watcher = wil::make_registry_watcher_nothrow(HKEY_CURRENT_USER, L"Software\\MyApp", true, wil::RegistryChangeKind[]
    //     {
    //         // invalidate cached registry data here
    //     });
    // RETURN_IF_NULL_ALLOC(watcher);

    enum class RegistryChangeKind
    {
        Modify = 0,
        Delete = 1,
    };

    /// @cond
    namespace details
    {
        struct registry_watcher_state
        {
            registry_watcher_state(unique_hkey&& keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
                : m_callback(wistd::move(callback)), m_keyToWatch(wistd::move(keyToWatch)), m_isRecursive(isRecursive)
            {
            }
            wistd::function<void(RegistryChangeKind)> m_callback;
            unique_hkey m_keyToWatch;
            unique_event_nothrow m_eventHandle;

            // While not strictly needed since this is ref counted the thread pool wait
            // should be last to ensure that the other members are valid
            // when it is destructed as it will reference them.
            unique_threadpool_wait m_threadPoolWait;
            bool m_isRecursive;

            volatile long m_refCount = 1;
            srwlock m_lock;

            // Returns true if the refcount can be increased from a non zero value,
            // false it was zero impling that the object is in or on the way to the destructor.
            // In this case ReleaseFromCallback() should not be called.
            bool TryAddRef()
            {
                return ::InterlockedIncrement(&m_refCount) > 1;
            }

            void Release()
            {
                auto lock = m_lock.lock_exclusive();
                if (0 == ::InterlockedDecrement(&m_refCount))
                {
                    lock.reset(); // leave the lock before deleting it.
                    delete this;
                }
            }

            void ReleaseFromCallback(bool rearm)
            {
                auto lock = m_lock.lock_exclusive();
                if (0 == ::InterlockedDecrement(&m_refCount))
                {
                    // Destroy the thread pool wait now to avoid the wait that would occur in the
                    // destructor. That wait would cause a deadlock since we are doing this from the callback.
                    ::CloseThreadpoolWait(m_threadPoolWait.release());
                    lock.reset(); // leave the lock before deleting it.
                    delete this;
                    // Sleep(1); // Enable for testing to find use after free bugs.
                }
                else if (rearm)
                {
                    ::SetThreadpoolWait(m_threadPoolWait.get(), m_eventHandle.get(), nullptr);
                }
            }
        };

        inline void delete_registry_watcher_state(_In_opt_ registry_watcher_state* watcherStorage) { watcherStorage->Release(); }

        typedef resource_policy<registry_watcher_state*, decltype(&details::delete_registry_watcher_state),
            details::delete_registry_watcher_state, details::pointer_access_none> registry_watcher_state_resource_policy;
    }
    /// @endcond

    template <typename storage_t, typename err_policy = err_exception_policy>
    class registry_watcher_t : public storage_t
    {
    public:
        // forward all base class constructors...
        template <typename... args_t>
        explicit registry_watcher_t(args_t&&... args) WI_NOEXCEPT : storage_t(wistd::forward<args_t>(args)...) {}

        // HRESULT or void error handling...
        typedef typename err_policy::result result;

        // Exception-based constructors
        registry_watcher_t(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create(rootKey, subKey, isRecursive, wistd::move(callback));
        }

        registry_watcher_t(unique_hkey&& keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create(wistd::move(keyToWatch), isRecursive, wistd::move(callback));
        }

        // Pass a root key, sub key pair or use an empty string to use rootKey as the key to watch.
        result create(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
        {
            // Most use will want to create the key, consider adding an option for open as a future design change.
            unique_hkey keyToWatch;
            HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyExW(rootKey, subKey, 0, nullptr, 0, KEY_NOTIFY, nullptr, &keyToWatch, nullptr));
            if (FAILED(hr))
            {
                return err_policy::HResult(hr);
            }
            return err_policy::HResult(create_common(wistd::move(keyToWatch), isRecursive, wistd::move(callback)));
        }

        result create(unique_hkey&& keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
        {
            return err_policy::HResult(create_common(wistd::move(keyToWatch), isRecursive, wistd::move(callback)));
        }

    private:
        // Factored into a standalone function to support Clang which does not support conversion of stateless lambdas
        // to __stdcall
        static void __stdcall callback(PTP_CALLBACK_INSTANCE, void* context, TP_WAIT*, TP_WAIT_RESULT)
        {
#ifndef __WIL_REGISTRY_CHANGE_CALLBACK_TEST
#define __WIL_REGISTRY_CHANGE_CALLBACK_TEST
#endif
            __WIL_REGISTRY_CHANGE_CALLBACK_TEST
                const auto watcherState = static_cast<details::registry_watcher_state*>(context);
            if (watcherState->TryAddRef())
            {
                // using auto reset event so don't need to manually reset.

                // failure here is a programming error.
                const LSTATUS error = RegNotifyChangeKeyValue(watcherState->m_keyToWatch.get(), watcherState->m_isRecursive,
                    REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_THREAD_AGNOSTIC,
                    watcherState->m_eventHandle.get(), TRUE);

                // Call the client before re-arming to ensure that multiple callbacks don't
                // run concurrently.
                switch (error)
                {
                case ERROR_SUCCESS:
                case ERROR_ACCESS_DENIED:
                    // Normal modification: send RegistryChangeKind::Modify and re-arm.
                    watcherState->m_callback(RegistryChangeKind::Modify);
                    watcherState->ReleaseFromCallback(true);
                    break;

                case ERROR_KEY_DELETED:
                    // Key deleted, send RegistryChangeKind::Delete, do not re-arm.
                    watcherState->m_callback(RegistryChangeKind::Delete);
                    watcherState->ReleaseFromCallback(false);
                    break;

                case ERROR_HANDLE_REVOKED:
                    // Handle revoked.  This can occur if the user session ends before
                    // the watcher shuts-down.  Disarm silently since there is generally no way to respond.
                    watcherState->ReleaseFromCallback(false);
                    break;

                default:
                    FAIL_FAST_HR(HRESULT_FROM_WIN32(error));
                }
            }
        }

        // This function exists to avoid template expansion of this code based on err_policy.
        HRESULT create_common(unique_hkey&& keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
        {
            wistd::unique_ptr<details::registry_watcher_state> watcherState(new(std::nothrow) details::registry_watcher_state(
                wistd::move(keyToWatch), isRecursive, wistd::move(callback)));
            RETURN_IF_NULL_ALLOC(watcherState);
            RETURN_IF_FAILED(watcherState->m_eventHandle.create());
            RETURN_IF_WIN32_ERROR(RegNotifyChangeKeyValue(watcherState->m_keyToWatch.get(),
                watcherState->m_isRecursive, REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_THREAD_AGNOSTIC,
                watcherState->m_eventHandle.get(), TRUE));

            watcherState->m_threadPoolWait.reset(CreateThreadpoolWait(&registry_watcher_t::callback, watcherState.get(), nullptr));
            RETURN_LAST_ERROR_IF(!watcherState->m_threadPoolWait);
            storage_t::reset(watcherState.release()); // no more failures after this, pass ownership
            SetThreadpoolWait(storage_t::get()->m_threadPoolWait.get(), storage_t::get()->m_eventHandle.get(), nullptr);
            return S_OK;
        }
    };

    typedef unique_any_t<registry_watcher_t<details::unique_storage<details::registry_watcher_state_resource_policy>, err_returncode_policy>> unique_registry_watcher_nothrow;
    typedef unique_any_t<registry_watcher_t<details::unique_storage<details::registry_watcher_state_resource_policy>, err_failfast_policy>> unique_registry_watcher_failfast;

    inline unique_registry_watcher_nothrow make_registry_watcher_nothrow(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback) WI_NOEXCEPT
    {
        unique_registry_watcher_nothrow watcher;
        watcher.create(rootKey, subKey, isRecursive, wistd::move(callback));
        return watcher; // caller must test for success using if (watcher)
    }

    inline unique_registry_watcher_nothrow make_registry_watcher_nothrow(unique_hkey&& keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback) WI_NOEXCEPT
    {
        unique_registry_watcher_nothrow watcher;
        watcher.create(wistd::move(keyToWatch), isRecursive, wistd::move(callback));
        return watcher; // caller must test for success using if (watcher)
    }

    inline unique_registry_watcher_failfast make_registry_watcher_failfast(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
    {
        return unique_registry_watcher_failfast(rootKey, subKey, isRecursive, wistd::move(callback));
    }

    inline unique_registry_watcher_failfast make_registry_watcher_failfast(unique_hkey&& keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
    {
        return unique_registry_watcher_failfast(wistd::move(keyToWatch), isRecursive, wistd::move(callback));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    typedef unique_any_t<registry_watcher_t<details::unique_storage<details::registry_watcher_state_resource_policy>, err_exception_policy >> unique_registry_watcher;

    inline unique_registry_watcher make_registry_watcher(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
    {
        return unique_registry_watcher(rootKey, subKey, isRecursive, wistd::move(callback));
    }

    inline unique_registry_watcher make_registry_watcher(unique_hkey&& keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)>&& callback)
    {
        return unique_registry_watcher(wistd::move(keyToWatch), isRecursive, wistd::move(callback));
    }
#endif // WIL_ENABLE_EXCEPTIONS
} // namespace wil

#endif
