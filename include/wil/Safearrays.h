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
#include <OleAuto.h>

namespace wil
{
#if defined( _OLEAUTO_H_ ) && !defined(__WIL_OLEAUTO_)
#define __WIL_OLEAUTO_
    /// @cond
    namespace details
    {
        template<typename T>
        struct TypeTraits{};

        template<> struct TypeTraits<char>                       { enum { type = VT_I1 }; };
        template<> struct TypeTraits<short>                      { enum { type = VT_I2 }; };
        template<> struct TypeTraits<long>                       { enum { type = VT_I4 }; };
        template<> struct TypeTraits<int>                        { enum { type = VT_I4 }; };
        template<> struct TypeTraits<long long>                  { enum { type = VT_I8 }; };
        template<> struct TypeTraits<__int64>                    { enum { type = VT_I8 }; };
        template<> struct TypeTraits<unsigned char>              { enum { type = VT_UI1 }; };
        template<> struct TypeTraits<unsigned short>             { enum { type = VT_UI2 }; };
        template<> struct TypeTraits<unsigned long>              { enum { type = VT_UI4 }; };
        template<> struct TypeTraits<unsigned int>               { enum { type = VT_UI4 }; };
        template<> struct TypeTraits<unsigned long long>         { enum { type = VT_UI8 }; };
        template<> struct TypeTraits<unsigned __int64>           { enum { type = VT_UI8 }; };
        template<> struct TypeTraits<float>                      { enum { type = VT_R4 }; };
        template<> struct TypeTraits<double>                     { enum { type = VT_R8 }; };

        template<typename INTERFACE>
        struct TypeTraits<com_ptr<INTERFACE>>                    { enum { type = VT_UNKNOWN }; };

        inline void __stdcall SafeArrayDestory(SAFEARRAY* psa) WI_NOEXCEPT
        {
            FAIL_FAST_IF_FAILED(::SafeArrayDestroy(psa));
        }

        inline void __stdcall SafeArrayUnlock(SAFEARRAY* psa) WI_NOEXCEPT
        {
            FAIL_FAST_IF_FAILED(::SafeArrayUnlock(psa));
        }

        inline void __stdcall SafeArrayUnaccessData(SAFEARRAY* psa) WI_NOEXCEPT
        {
            FAIL_FAST_IF_FAILED(::SafeArrayUnaccessData(psa));
        }

        typedef resource_policy<SAFEARRAY*, decltype(&details::SafeArrayDestory), details::SafeArrayDestory, details::pointer_access_all> safearray_resource_policy;
        //typedef resource_policy<SAFEARRAY*, decltype(&details::SafeArrayUnlock), details::SafeArrayUnlock, details::pointer_access_all> safearray_lock_resource_policy;
        //typedef resource_policy<SAFEARRAY*, decltype(&details::SafeArrayUnaccessData), details::SafeArrayUnaccessData, details::pointer_access_all> safearray_accessdata_resource_policy;

    }
    /// @endcond

    typedef unique_any<SAFEARRAY*, decltype(&details::SafeArrayUnlock), details::SafeArrayUnlock, details::pointer_access_noaddress> safearray_unlock_scope_exit;

    // Guarantees a SafeArrayUnlock call on the given object when the returned object goes out of scope
    // Note: call SafeArrayUnlock early with the reset() method on the returned object or abort the call with the release() method
    WI_NODISCARD inline safearray_unlock_scope_exit SafeArrayUnlock_scope_exit(SAFEARRAY* psa) WI_NOEXCEPT
    {
        __FAIL_FAST_ASSERT__(psa != nullptr);
        return safearray_unlock_scope_exit(psa);
    }

    template <typename storage_t, typename err_policy = err_exception_policy>
    class safearray_t : public storage_t
    {
    public:
        // forward all base class constructors...
        template <typename... args_t>
        explicit safearray_t(args_t&&... args) WI_NOEXCEPT : storage_t(wistd::forward<args_t>(args)...) {}

        // HRESULT or void error handling...
        typedef typename err_policy::result result;

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
                RETURN_HR_IF_NULL(E_INVALIDARG, sab);
                RETURN_HR_IF(E_INVALIDARG, cDims < 1);
                SAFEARRAY* sa = ::SafeArrayCreate(vt, cDims, sab);
                RETURN_LAST_ERROR_IF_NULL(sa);
                storage_t::reset(sa);
                return S_OK;
            }();

            return err_policy::HResult(hr);
        }

        // Single Dimension specialization
        result create(VARTYPE vt, UINT cElements, LONG lowerBound = 0 )
        {
            auto bounds = SAFEARRAYBOUND{cElements, lowerBound};

            return err_policy::HResult(create(vt, 1, &bounds));
        }

        ULONG dim() const WI_NOEXCEPT { return ::SafeArrayGetDim(storage_t::get()); }

        result lbound(UINT nDim, LONG* pLbound) const
        {
            return err_policy::HResult(::SafeArrayGetLBound(storage_t::get(), nDim, pLbound));
        }
        result lbound(LONG* pLbound) const
        {
            return err_policy::HResult(::SafeArrayGetLBound(storage_t::get(), 1, pLbound));
        }
        result ubound(UINT nDim, LONG* pUbound) const
        {
            return err_policy::HResult(::SafeArrayGetUBound(storage_t::get(), nDim, pUbound));
        }
        result ubound(LONG* pUbound) const
        {
            return err_policy::HResult(::SafeArrayGetUBound(storage_t::get(), 1, pUbound));
        }

        // Lock Helper
        WI_NODISCARD safearray_unlock_scope_exit scope_lock() const WI_NOEXCEPT
        {
            return wil::SafeArrayUnlock_scope_exit(storage_t::get());
        }

        // Exception-based helper functions
        LONG lbound(UINT nDim = 1) const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            LONG result = 0;
            lbound(nDim, &result);
            return result;
        }
        LONG ubound(UINT nDim = 1) const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            LONG result = 0;
            ubound(nDim, &result);
            return result;
        }


    private:

    };



#ifdef WIL_ENABLE_EXCEPTIONS
#endif // WIL_ENABLE_EXCEPTIONS
} // namespace wil

#endif
#endif
