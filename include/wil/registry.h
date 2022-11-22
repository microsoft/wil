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
#include "resource.h" // unique_hkey

namespace wil
{
    //! The key name includes the absolute path of the key in the registry, always starting at a
    //! base key, for example, HKEY_LOCAL_MACHINE.
    size_t const max_registry_key_name_length = 255;

    //! The maximum number of characters allowed in a registry value's name.
    size_t const max_registry_value_name_length = 16383;

#ifdef WIL_ENABLE_EXCEPTIONS
    inline wil::unique_hkey open_registry_key(HKEY hkey, PCWSTR subkey, DWORD options = 0, REGSAM samDesired = KEY_READ)
    {
        wil::unique_hkey openKey;
        THROW_IF_FAILED(HRESULT_FROM_WIN32(::RegOpenKeyExW(hkey, subkey, options, samDesired, &openKey)));
        return openKey;
    }

    // TODO: can we make any of these accesses simpler?
    inline wil::unique_hkey create_registry_key(
        HKEY hkey,
        PCWSTR subkey,
        PWSTR classType = nullptr,
        DWORD options = REG_OPTION_NON_VOLATILE,
        REGSAM samDesired = KEY_READ,
        SECURITY_ATTRIBUTES* securityAttributes = nullptr,
        _Out_ DWORD* disposition = nullptr)
    {
        wil::unique_hkey openKey;
        THROW_IF_FAILED(HRESULT_FROM_WIN32(::RegCreateKeyExW(hkey, subkey, 0, classType, options, samDesired, securityAttributes, &openKey, disposition)));
        return openKey;
    }
#endif // WIL_ENABLE_EXCEPTIONS

    // TODO: support "any" function that deduces type?

    inline HRESULT get_registry_dword_nothrow(HKEY hkey, LPCWSTR subkey, LPCWSTR regValueName, _Out_ DWORD* value) noexcept
    {
        DWORD size{sizeof(value)};
        // TODO: support internal registry functions?
        // TODO: support additional flags, like RRF_ZEROONFAILURE?
        // TODO: convert to HRESULT?
        const auto hr = HRESULT_FROM_WIN32(::RegGetValueW(hkey, subkey, regValueName, RRF_RT_DWORD, nullptr, value, &size));
        return hr;
    }

    /// @cond
    namespace details
    {
        template <typename err_policy, typename result = err_policy::result>
        result set_registry_dword(HKEY hkey, LPCWSTR subkey, LPCWSTR regValueName, DWORD value)
        {
            // TODO: support little endian and big endian?
            const auto hr = HRESULT_FROM_WIN32(::RegSetKeyValueW(hkey, subkey, regValueName, REG_DWORD, &value, sizeof(value)));
            return err_policy::HResult(hr);
        }
    }
    /// @endcond

    constexpr auto* set_registry_dword_nothrow = details::set_registry_dword<err_returncode_policy>;
    constexpr auto* set_registry_dword_failfast = details::set_registry_dword<err_failfast_policy>;

#ifdef WIL_ENABLE_EXCEPTIONS

    constexpr auto* set_registry_dword = details::set_registry_dword<err_exception_policy>;

    /// @cond
    namespace details
    {
#if defined(_OPTIONAL_)
        template <typename Result>
        struct optional_registry_policy
        {
            typedef std::optional<Result> result;
            __forceinline static result NotFound() { return std::nullopt; }
        };
#endif // defined(_OPTIONAL_)

        template<typename Result>
        struct required_registry_policy
        {
            typedef Result result;
            __forceinline static result NotFound()
            {
                THROW_HR(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
            }
        };

        template <typename policy, typename result_t = policy::result>
        result_t get_registry_dword(HKEY hkey, LPCWSTR subkey = nullptr, LPCWSTR regValueName = nullptr)
        {
            DWORD value{};
            const auto result = get_registry_dword_nothrow(hkey, subkey, regValueName, &value);
            if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                return policy::NotFound();
            }
            THROW_IF_FAILED(result);
            return value;
        }

#if defined(_STRING_)
        template<typename policy, typename result_t = policy::result>
        result_t get_registry_string(HKEY hkey, LPCWSTR subkey = nullptr, LPCWSTR regValueName = nullptr)
        {
            // TODO: type RRF_RT_REG_EXPAND_SZ etc?
            // https://docs.microsoft.com/en-us/archive/msdn-magazine/2017/may/c-use-modern-c-to-access-the-windows-registry#reading-a-string-value-from-the-registry
            DWORD size{};
            const auto sizeResult = ::RegGetValueW(hkey, subkey, regValueName, RRF_RT_REG_SZ, nullptr, nullptr, &size);
            if (sizeResult == ERROR_FILE_NOT_FOUND)
            {
                return policy::NotFound();
            }
            THROW_IF_FAILED(HRESULT_FROM_WIN32(sizeResult));

            std::wstring data{};
            data.resize(size / sizeof(wchar_t));
            const auto result = ::RegGetValueW(hkey, subkey, regValueName, RRF_RT_REG_SZ, nullptr, &data[0], &size);

            // We read *something* before, so any error should be a failure.
            THROW_IF_FAILED(HRESULT_FROM_WIN32(result));

            // Get rid of the double null
            DWORD stringLengthInWchars = (size / sizeof(wchar_t)) - 1;
            data.resize(stringLengthInWchars);

            return data;
        }
#endif // defined(_STRING_)
    }
    /// @endcond

#if defined(_OPTIONAL_)
    constexpr auto* try_get_registry_dword = details::get_registry_dword<details::optional_registry_policy<DWORD>>;
#endif // defined(_OPTIONAL_)
    constexpr auto* get_registry_dword = details::get_registry_dword<details::required_registry_policy<DWORD>>;

#if defined(_STRING_)
#if defined(_OPTIONAL_)
    constexpr auto* try_get_registry_string = details::get_registry_string<details::optional_registry_policy<std::wstring>>;
#endif // defined(_OPTIONAL_)
    constexpr auto* get_registry_string = details::get_registry_string<details::required_registry_policy<std::wstring>>;
#endif // defined(_STRING_)

#endif  // WIL_ENABLE_EXCEPTIONS

#if defined(_STRING_)
    /// @cond
    namespace details
    {
        template <typename err_policy, typename result = err_policy::result>
        result set_registry_string(HKEY hkey, LPCWSTR subkey, LPCWSTR regValueName, const std::wstring& value)
        {
            static_assert(wistd::is_same_v<std::wstring::value_type, wchar_t>, "We assume a wstring is made of wchar_ts");

            // TODO: where does the +1 go?
            // TODO: check for DWORD/size_t overflow?
            const DWORD byteSize = (static_cast<DWORD>(value.size()) * sizeof(wchar_t)) + 1;
            const auto hr = HRESULT_FROM_WIN32(::RegSetKeyValueW(hkey, subkey, regValueName, REG_SZ, value.c_str(), byteSize));
            return err_policy::HResult(hr);
        }
    }
    /// @endcond

#ifdef WIL_ENABLE_EXCEPTIONS
    constexpr auto* set_registry_string = details::set_registry_string<err_exception_policy>;
#endif  // WIL_ENABLE_EXCEPTIONS
    constexpr auto* set_registry_string_nothrow = details::set_registry_string<err_returncode_policy>;
    constexpr auto* set_registry_string_failfast = details::set_registry_string<err_failfast_policy>;
#endif // defined(_STRING_)

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
            registry_watcher_state(unique_hkey &&keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)> &&callback)
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

        inline void delete_registry_watcher_state(_In_opt_ registry_watcher_state *watcherStorage) { watcherStorage->Release(); }

        typedef resource_policy<registry_watcher_state *, decltype(&details::delete_registry_watcher_state),
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
        registry_watcher_t(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)> &&callback)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create(rootKey, subKey, isRecursive, wistd::move(callback));
        }

        registry_watcher_t(unique_hkey &&keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)> &&callback)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create(wistd::move(keyToWatch), isRecursive, wistd::move(callback));
        }

        // Pass a root key, sub key pair or use an empty string to use rootKey as the key to watch.
        result create(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)> &&callback)
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

        result create(unique_hkey &&keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)> &&callback)
        {
            return err_policy::HResult(create_common(wistd::move(keyToWatch), isRecursive, wistd::move(callback)));
        }

    private:
        // Factored into a standalone function to support Clang which does not support conversion of stateless lambdas
        // to __stdcall
        static void __stdcall callback(PTP_CALLBACK_INSTANCE, void *context, TP_WAIT *, TP_WAIT_RESULT)
        {
#ifndef __WIL_REGISTRY_CHANGE_CALLBACK_TEST
#define __WIL_REGISTRY_CHANGE_CALLBACK_TEST
#endif
            __WIL_REGISTRY_CHANGE_CALLBACK_TEST
            auto watcherState = static_cast<details::registry_watcher_state *>(context);
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
        HRESULT create_common(unique_hkey &&keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)> &&callback)
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

    inline unique_registry_watcher_nothrow make_registry_watcher_nothrow(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)>  &&callback) WI_NOEXCEPT
    {
        unique_registry_watcher_nothrow watcher;
        watcher.create(rootKey, subKey, isRecursive, wistd::move(callback));
        return watcher; // caller must test for success using if (watcher)
    }

    inline unique_registry_watcher_nothrow make_registry_watcher_nothrow(unique_hkey &&keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)>  &&callback) WI_NOEXCEPT
    {
        unique_registry_watcher_nothrow watcher;
        watcher.create(wistd::move(keyToWatch), isRecursive, wistd::move(callback));
        return watcher; // caller must test for success using if (watcher)
    }

    inline unique_registry_watcher_failfast make_registry_watcher_failfast(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)>  &&callback)
    {
        return unique_registry_watcher_failfast(rootKey, subKey, isRecursive, wistd::move(callback));
    }

    inline unique_registry_watcher_failfast make_registry_watcher_failfast(unique_hkey &&keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)>  &&callback)
    {
        return unique_registry_watcher_failfast(wistd::move(keyToWatch), isRecursive, wistd::move(callback));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    typedef unique_any_t<registry_watcher_t<details::unique_storage<details::registry_watcher_state_resource_policy>, err_exception_policy >> unique_registry_watcher;

    inline unique_registry_watcher make_registry_watcher(HKEY rootKey, _In_ PCWSTR subKey, bool isRecursive, wistd::function<void(RegistryChangeKind)>  &&callback)
    {
        return unique_registry_watcher(rootKey, subKey, isRecursive, wistd::move(callback));
    }

    inline unique_registry_watcher make_registry_watcher(unique_hkey &&keyToWatch, bool isRecursive, wistd::function<void(RegistryChangeKind)> &&callback)
    {
        return unique_registry_watcher(wistd::move(keyToWatch), isRecursive, wistd::move(callback));
    }
#endif // WIL_ENABLE_EXCEPTIONS
} // namespace wil

#endif
