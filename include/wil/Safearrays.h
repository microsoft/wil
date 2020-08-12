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
#ifndef __WIL_SAFEARRAYS_INCLUDED
#define __WIL_SAFEARRAYS_INCLUDED

#ifdef _KERNEL_MODE
#error This header is not supported in kernel-mode.
#endif

#include <new.h> // new(std::nothrow)
#include "resource.h" // unique_hkey

namespace wil
{
#if defined( _OLEAUTO_H_ ) && !defined(__WIL_OLEAUTO_)
#define __WIL_OLEAUTO_
    /// @cond
    namespace details
    {
        template<typename T>
        struct VarTraits{};

        template<> struct VarTraits<char>                       { enum { type = VT_I1 }; };
        template<> struct VarTraits<short>                      { enum { type = VT_I2 }; };
        template<> struct VarTraits<long>                       { enum { type = VT_I4 }; };
        template<> struct VarTraits<int>                        { enum { type = VT_I4 }; };
        template<> struct VarTraits<long long>                  { enum { type = VT_I8 }; };
        template<> struct VarTraits<unsigned char>              { enum { type = VT_UI1 }; };
        template<> struct VarTraits<unsigned short>             { enum { type = VT_UI2 }; };
        template<> struct VarTraits<unsigned long>              { enum { type = VT_UI4 }; };
        template<> struct VarTraits<unsigned int>               { enum { type = VT_UI4 }; };
        template<> struct VarTraits<unsigned long long>         { enum { type = VT_UI8 }; };
        template<> struct VarTraits<float>                      { enum { type = VT_R4 }; };
        template<> struct VarTraits<double>                     { enum { type = VT_R8 }; };
        template<> struct VarTraits<BSTR>                       { enum { type = VT_BSTR }; };
        template<> struct VarTraits<LPUNKNOWN>                  { enum { type = VT_UNKNOWN }; };
        template<> struct VarTraits<VARIANT>                    { enum { type = VT_VARIANT }; };

        inline void __stdcall SafeArrayDestory(SAFEARRAY* psa) WI_NOEXCEPT
        {
            __FAIL_FAST_ASSERT__(psa != nullptr);
            FAIL_FAST_IF_FAILED(::SafeArrayDestroy(psa));
        }

        inline void __stdcall SafeArrayLock(SAFEARRAY* psa) WI_NOEXCEPT
        {
            __FAIL_FAST_ASSERT__(psa != nullptr);
            FAIL_FAST_IF_FAILED(::SafeArrayLock(psa));
        }

        inline void __stdcall SafeArrayUnlock(SAFEARRAY* psa) WI_NOEXCEPT
        {
            __FAIL_FAST_ASSERT__(psa != nullptr);
            FAIL_FAST_IF_FAILED(::SafeArrayUnlock(psa));
        }

        inline void __stdcall SafeArrayUnaccessData(SAFEARRAY* psa) WI_NOEXCEPT
        {
            __FAIL_FAST_ASSERT__(psa != nullptr);
            FAIL_FAST_IF_FAILED(::SafeArrayUnaccessData(psa));
        }

        inline HRESULT __stdcall SafeArrayCreate(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* sab, SAFEARRAY*& psa) WI_NOEXCEPT
        {
            WI_ASSERT(sab != nullptr);
            WI_ASSERT(cDims > 0);
            psa = ::SafeArrayCreate(vt, cDims, sab);
            RETURN_LAST_ERROR_IF_NULL(psa);
            return S_OK;
        }

        inline HRESULT __stdcall SafeArrayCountElements(SAFEARRAY* psa, ULONG* pCount) WI_NOEXCEPT
        {
            RETURN_HR_IF_NULL(E_POINTER, pCount);

            if ( psa != nullptr )
            {
                ULONGLONG result = 1;
                for (UINT i = 0; i < psa->cDims; ++i)
                {
                    result *= psa->rgsabound[i].cElements;
                    if (result > ULONG_MAX)
                    {
                        RETURN_HR(HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
                    }
                }

                *pCount = static_cast<ULONG>(result);
            }
            else
            {
                // If it's invalid, it doesn't contain any elements
                *pCount = 0;
            }
            return S_OK;
        }

        typedef resource_policy<SAFEARRAY*, decltype(&details::SafeArrayDestory), details::SafeArrayDestory, details::pointer_access_all> safearray_resource_policy;
        typedef resource_policy<SAFEARRAY*, decltype(&details::SafeArrayUnaccessData), details::SafeArrayUnaccessData, details::pointer_access_all> safearray_accessdata_resource_policy;

    }
    /// @endcond

    typedef unique_any<SAFEARRAY*, decltype(&details::SafeArrayUnlock), details::SafeArrayUnlock, details::pointer_access_noaddress> safearray_unlock_scope_exit;

    // Guarantees a SafeArrayUnlock call on the given object when the returned object goes out of scope
    // Note: call SafeArrayUnlock early with the reset() method on the returned object or abort the call with the release() method
    WI_NODISCARD inline safearray_unlock_scope_exit SafeArrayUnlock_scope_exit(SAFEARRAY* psa) WI_NOEXCEPT
    {
        details::SafeArrayLock(psa);
        return safearray_unlock_scope_exit(psa);
    }

    template <typename T, typename storage_t, typename err_policy = err_exception_policy>
    class safearraydata_t : public storage_t
    {
    public:
        typedef typename err_policy::result result;
        typedef typename storage_t::pointer pointer;

    public:
        // forward all base class constructors...
        template <typename... args_t>
        explicit safearraydata_t(args_t&&... args) WI_NOEXCEPT : storage_t(wistd::forward<args_t>(args)...) {}

        // Exception-based construction
        safearraydata_t(pointer psa)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create(psa);
        }

        result create(pointer psa)
        {
            HRESULT hr = [&]()
            {
                WI_ASSERT(sizeof(T) == ::SafeArrayGetElemsize(psa));
                RETURN_HR_IF(E_INVALIDARG, !storage_t::is_valid(psa));
                RETURN_IF_FAILED(::SafeArrayAccessData(psa, &m_pBegin));
                storage_t::reset(psa);
                RETURN_IF_FAILED(details::SafeArrayCountElements(storage_t::get(), &m_nCount));
                return S_OK;
            }();

            return err_policy::HResult(hr);
        }

        // Ranged-for style
        T* begin() WI_NOEXCEPT { return m_pBegin; }
        T* end() WI_NOEXCEPT { return m_pBegin+m_nCount; }
        const T* begin() const WI_NOEXCEPT { return m_pBegin; }
        const T* end() const WI_NOEXCEPT { return m_pBegin+m_nCount; }

        // Old style iteration
        ULONG count() const WI_NOEXCEPT { return m_nCount; }
        WI_NODISCARD T& operator[](ULONG i) { WI_ASSERT(i < m_nCount);  return *(m_pBegin + i); }
        WI_NODISCARD const T& operator[](ULONG i) const { WI_ASSERT(i < m_nCount); return *(m_pBegin +i); }

    private:
        T*  m_pBegin = nullptr;
        ULONG m_nCount = 0;
    };

    template<typename T>
    using unique_safearrayaccess_nothrow = unique_any_t<safearraydata_t<T, details::unique_storage<details::safearray_accessdata_resource_policy>, err_returncode_policy>>;
    template<typename T>
    using unique_safearrayaccess_failfast = unique_any_t<safearraydata_t<T, details::unique_storage<details::safearray_accessdata_resource_policy>, err_failfast_policy>>;

#ifdef WIL_ENABLE_EXCEPTIONS
     template<typename T>
     using unique_safearrayaccess = unique_any_t<safearraydata_t<T, details::unique_storage<details::safearray_accessdata_resource_policy>, err_exception_policy>>;
#endif


    template <typename storage_t, typename err_policy = err_exception_policy>
    class safearray_t : public storage_t
    {
    public:
        template<typename T>
        using unique_accessdata_t = unique_any_t<safearraydata_t<T, details::unique_storage<details::safearray_accessdata_resource_policy>, err_policy>>;

        using unique_safearray_t = unique_any_t<safearray_t<storage_t, err_policy>>;
        typedef typename err_policy::result result;
        typedef typename storage_t::pointer pointer;

    public:
        // forward all base class constructors...
        template <typename... args_t>
        explicit safearray_t(args_t&&... args) WI_NOEXCEPT : storage_t(wistd::forward<args_t>(args)...) {}

        // Exception-based construction
        safearray_t(VARTYPE vt, UINT cElements, LONG lowerBound = 0)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create(vt, cElements, lowerBound);
        }

        // Low-level, arbitrary number of dimensions
        result create(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* sab)
        {
            HRESULT hr = [&]()
            {
                auto psa = storage_t::policy::invalid_value();

                RETURN_IF_FAILED(details::SafeArrayCreate(vt, cDims, sab, psa));
                storage_t::reset(psa);
                return S_OK;
            }();

            return err_policy::HResult(hr);
        }

        // Single Dimension specialization
        result create(VARTYPE vt, UINT cElements, LONG lowerBound = 0)
        {
            HRESULT hr = [&]()
            {
                auto bounds = SAFEARRAYBOUND{ cElements, lowerBound };
                auto psa = storage_t::policy::invalid_value();

                RETURN_IF_FAILED(details::SafeArrayCreate(vt, 1, &bounds, psa));
                storage_t::reset(psa);
                return S_OK;
            }();

            return err_policy::HResult(hr);
        }
        template<typename T>
        result create(UINT cElements, LONG lowerBound = 0)
        {
            constexpr auto vt = static_cast<VARTYPE>(details::VarTraits<T>::type);
            HRESULT hr = [&]()
            {
                auto bounds = SAFEARRAYBOUND{ cElements, lowerBound };
                auto psa = storage_t::policy::invalid_value();

                RETURN_IF_FAILED(details::SafeArrayCreate(vt, 1, &bounds, psa));
                storage_t::reset(psa);
                return S_OK;
            }();

            return err_policy::HResult(hr);
        }

        // Create a Copy
        result create(pointer psaSrc)
        {
            HRESULT hr = [&]()
            {
               auto psa = storage_t::policy::invalid_value();

                RETURN_IF_FAILED(::SafeArrayCopy(psaSrc, &psa));
                storage_t::reset(psa);
                return S_OK;
            }();

            return err_policy::HResult(hr);
        }

        // Dimensions, Sizes, Boundaries
        ULONG dim() const WI_NOEXCEPT { return ::SafeArrayGetDim(storage_t::get()); }
        ULONG elemsize() const WI_NOEXCEPT { return ::SafeArrayGetElemsize(storage_t::get()); }
        result lbound(UINT nDim, LONG* pLbound) const
        {
            return err_policy::HResult(::SafeArrayGetLBound(storage_t::get(), nDim, pLbound));
        }
        result lbound(LONG* pLbound) const
        {
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayGetLBound(storage_t::get(), 1, pLbound));
        }
        result ubound(UINT nDim, LONG* pUbound) const
        {
            return err_policy::HResult(::SafeArrayGetUBound(storage_t::get(), nDim, pUbound));
        }
        result ubound(LONG* pUbound) const
        {
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayGetUBound(storage_t::get(), 1, pUbound));
        }
        result count(ULONG* pCount) const
        {
            return err_policy::HResult(details::SafeArrayCountElements(storage_t::get(), pCount));
        }

        // Lock Helper
        WI_NODISCARD safearray_unlock_scope_exit scope_lock() const WI_NOEXCEPT
        {
            return wil::SafeArrayUnlock_scope_exit(storage_t::get());
        }

        result put_element(void* pv, LONG* pIndices)
        {
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), pIndices, pv));
        }
        result put_element(void* pv, LONG nIndex)
        {
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), &nIndex, pv));
        }
        template<typename T>
        result put_element(T t, LONG* pIndices)
        {
            WI_ASSERT(sizeof(t) == elemsize());
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), pIndices, &t));
        }
        template<typename T>
        result put_element(T t, LONG nIndex)
        {
            WI_ASSERT(sizeof(t) == elemsize());
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), &nIndex, &t));
        }

        result get_element(void* pv, LONG* pIndices)
        {
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), pIndices, pv));
        }
        result get_element(void* pv, LONG nIndex)
        {
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), &nIndex, pv));
        }
        template<typename T>
        result get_element(T& t, LONG* pIndices)
        {
            WI_ASSERT(sizeof(t) == elemsize());
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), pIndices, &t));
        }
        template<typename T>
        result get_element(T& t, LONG nIndex)
        {
            WI_ASSERT(sizeof(t) == elemsize());
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), &nIndex, &t));
        }

        // Exception-based helper functions
        WI_NODISCARD LONG lbound(UINT nDim = 1) const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            LONG result = 0;
            lbound(nDim, &result);
            return result;
        }
        WI_NODISCARD LONG ubound(UINT nDim = 1) const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            LONG result = 0;
            ubound(nDim, &result);
            return result;
        }
        WI_NODISCARD ULONG count() const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            ULONG result = 0;
            count(&result);
            return result;
        }
        WI_NODISCARD unique_safearray_t copy() const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");

            auto result = unique_safearray_t{};
            result.create(storage_t::get());
            return result;
        }

    private:
    };

    using unique_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy>>;
    using unique_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy>>;

#ifdef WIL_ENABLE_EXCEPTIONS
    using unique_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy>>;
#endif

#endif

#ifdef WIL_ENABLE_EXCEPTIONS
#endif // WIL_ENABLE_EXCEPTIONS
} // namespace wil

#endif

