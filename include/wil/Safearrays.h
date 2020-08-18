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
#include "wistd_type_traits.h"
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
        //template<> struct VarTraits<short>                      { enum { type = VT_I2 }; };
        template<> struct VarTraits<long>                       { enum { type = VT_I4 }; };
        template<> struct VarTraits<int>                        { enum { type = VT_I4 }; };
        template<> struct VarTraits<long long>                  { enum { type = VT_I8 }; };
        template<> struct VarTraits<unsigned char>              { enum { type = VT_UI1 }; };
        template<> struct VarTraits<unsigned short>             { enum { type = VT_UI2 }; };
        template<> struct VarTraits<unsigned long>              { enum { type = VT_UI4 }; };
        template<> struct VarTraits<unsigned int>               { enum { type = VT_UI4 }; };
        template<> struct VarTraits<unsigned long long>         { enum { type = VT_UI8 }; };
        template<> struct VarTraits<float>                      { enum { type = VT_R4 }; };
        //template<> struct VarTraits<double>                     { enum { type = VT_R8 }; };
        template<> struct VarTraits<VARIANT_BOOL>               { enum { type = VT_BOOL }; };
        template<> struct VarTraits<DATE>                       { enum { type = VT_DATE }; };
        template<> struct VarTraits<CURRENCY>                   { enum { type = VT_CY }; };
        template<> struct VarTraits<DECIMAL>                    { enum { type = VT_DECIMAL }; };
        template<> struct VarTraits<BSTR>                       { enum { type = VT_BSTR }; };
        template<> struct VarTraits<LPUNKNOWN>                  { enum { type = VT_UNKNOWN }; };
        template<> struct VarTraits<LPDISPATCH>                 { enum { type = VT_DISPATCH }; };
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

        template<typename T>
        inline void __stdcall SafeArrayAccessData(SAFEARRAY* psa, T*& p) WI_NOEXCEPT
        {
            __FAIL_FAST_ASSERT__(psa != nullptr);
            FAIL_FAST_IF_FAILED(::SafeArrayAccessData(psa, reinterpret_cast<void**>(&p)));
        }

        inline void __stdcall SafeArrayUnaccessData(SAFEARRAY* psa) WI_NOEXCEPT
        {
            __FAIL_FAST_ASSERT__(psa != nullptr);
            FAIL_FAST_IF_FAILED(::SafeArrayUnaccessData(psa));
        }

        inline VARTYPE __stdcall SafeArrayGetVartype(SAFEARRAY* psa)
        {
            // Safearrays cannot hold type VT_NULL, so this value
            // means the SAFEARRAY was null
            VARTYPE vt = VT_NULL;
            if (psa != nullptr)
            {
                if (FAILED(::SafeArrayGetVartype(psa, &vt)))
                {
                    // Safearrays cannot hold type VT_EMPTY, so this value
                    // means the type couldn't be determined
                    vt = VT_EMPTY;
                }
            }
            return vt;
        }

        inline HRESULT __stdcall SafeArrayCreate(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* sab, SAFEARRAY*& psa) WI_NOEXCEPT
        {
            WI_ASSERT(sab != nullptr);
            WI_ASSERT(cDims > 0);
            psa = ::SafeArrayCreate(vt, cDims, sab);
            RETURN_LAST_ERROR_IF_NULL(psa);
            WI_ASSERT(vt == details::SafeArrayGetVartype(psa));
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

        //template<LONG... Indexes>
        //class safearray_index : private std::array<LONG, sizeof...(Indexes)>
        //{
        //public:
        //    safearray_index() : std::array<LONG, sizeof...(Indexes)>{ Indexes... };
        //    LPLONG get() { return data(); }
        //    ULONG count() { return static_cast<ULONG>(size()); }
        //};

        //template<LONG Index>
        //class safearray_index
        //{
        //public:
        //    safearray_index() : _value{ Index } {}
        //    LPLONG get() { return &_value; }
        //    ULONG count() { return 1; }
        //private:
        //    LONG _value;
        //};

        //template<LPLONG Indexes>
        //class safearray_index
        //{
        //public:
        //    safearray_index() : _indexes{Indexes}
        //    LPLONG get() { return _indexes; }
        //    ULONG count() { return 0; }
        //private:
        //    LPLONG _indexes:
        //};

        //template<typename... Args>
        //DebugContextT& Format(const wchar_t* const lpszFormat, Args... args) noexcept
        //    template<typename T, CharT... cs>
        //    constexpr auto chars = std::array<CharT, sizeof...(cs)>{ cs... };

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
        typedef T value_type;
        typedef value_type* value_pointer;
        typedef const value_type* const_value_pointer;
        typedef value_type& value_reference;
        typedef const value_type& const_value_reference;
        typedef value_type* iterator;
        typedef const value_type* const_iterator;

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
        safearraydata_t(safearraydata_t&& other) : storage_t(wistd::move(other))
        {
            m_pBegin = wistd::exchange(other.m_pBegin, nullptr);
            m_nSize = wistd::exchange(other.m_nSize, 0L);
        }
        ~safearraydata_t()
        {

        }

        result create(pointer psa)
        {
            HRESULT hr = [&]()
            {
                constexpr auto vt = static_cast<VARTYPE>(details::VarTraits<T>::type);
                WI_ASSERT(sizeof(T) == ::SafeArrayGetElemsize(psa));
                WI_ASSERT(vt == details::SafeArrayGetVartype(psa));
                details::SafeArrayAccessData(psa, m_pBegin);
                storage_t::reset(psa);
                RETURN_IF_FAILED(details::SafeArrayCountElements(storage_t::get(), &m_nSize));
                return S_OK;
            }();

            return err_policy::HResult(hr);
        }

        // Ranged-for style
        iterator begin() WI_NOEXCEPT { WI_ASSERT(m_pBegin != nullptr); return m_pBegin; }
        iterator end() WI_NOEXCEPT { WI_ASSERT(m_pBegin != nullptr); return m_pBegin+m_nSize; }
        const_iterator begin() const WI_NOEXCEPT { return m_pBegin; }
        const_iterator end() const WI_NOEXCEPT { return m_pBegin+m_nSize; }

        // Old style iteration
        ULONG size() const WI_NOEXCEPT { return m_nSize; }
        WI_NODISCARD value_reference operator[](ULONG i) { WI_ASSERT(i < m_nSize); return *(m_pBegin + i); }
        WI_NODISCARD const_value_reference operator[](ULONG i) const { WI_ASSERT(i < m_nSize); return *(m_pBegin +i); }

        // Utilities
        bool empty() const WI_NOEXCEPT { return m_nSize == ULONG{}; }

    private:
        value_pointer m_pBegin = nullptr;
        ULONG m_nSize = 0;
    };

    template<typename T>
    using unique_safearrayaccess_nothrow = unique_any_t<safearraydata_t<T, details::unique_storage<details::safearray_accessdata_resource_policy>, err_returncode_policy>>;
    template<typename T>
    using unique_safearrayaccess_failfast = unique_any_t<safearraydata_t<T, details::unique_storage<details::safearray_accessdata_resource_policy>, err_failfast_policy>>;

#ifdef WIL_ENABLE_EXCEPTIONS
     template<typename T>
     using unique_safearrayaccess = unique_any_t<safearraydata_t<T, details::unique_storage<details::safearray_accessdata_resource_policy>, err_exception_policy>>;
#endif


    // Add safearray functionality to the given storage type using the given error policy
    //  element_t is the either:
    //      A) The C++ type for the elements in the safearray and all methods are typesafe
    //      B) void to make the safearray generic (and not as typesafe)
    template <typename storage_t, typename err_policy = err_exception_policy, typename element_t = void>
    class safearray_t : public storage_t
    {
    private:
        // SFINAE Helpers to improve readability
        // Filters functions that don't require a type because a type was provided by the class type
        template<typename T> using EnableIfTyped = typename wistd::enable_if<!wistd::is_same<T, void>::value, int>::type;
        // Filters functions that require a type because the class type doesn't provide one (element_t == void)
        template<typename T> using EnableIfNotTyped = typename wistd::enable_if<wistd::is_same<T, void>::value, int>::type;
        template<typename T>
        using is_pointer_type = wistd::disjunction<wistd::is_same<T, BSTR>,
                                                    wistd::is_same<T, LPUNKNOWN>,
                                                    wistd::is_same<T, LPDISPATCH>>;

        template<typename T>
        using is_type_not_set = wistd::is_same<T, void>;

        template<typename T>
        using is_valid_type = wistd::negation<is_type_not_set<T>>;

        template <typename T>
        using is_value_type = wistd::conjunction<wistd::negation<is_pointer_type<T>>, is_valid_type<T>>;


    public:
        // AccessData type that still requires a type (used when not typed)
        template<typename T>
        using unique_accessdata_t = unique_any_t<safearraydata_t<T, details::unique_storage<details::safearray_accessdata_resource_policy>, err_policy>>;
        // AccessData type that uses the safearray's type (used when a type is provided)
        using unique_accessdata = unique_any_t<safearraydata_t<element_t, details::unique_storage<details::safearray_accessdata_resource_policy>, err_policy>>;

        // Represents this type
        using unique_type = unique_any_t<safearray_t<storage_t, err_policy, element_t>>;

        typedef typename err_policy::result result;
        typedef typename storage_t::pointer pointer;
        typedef element_t elemtype;

    public:
        // forward all base class constructors...
        template <typename... args_t>
        explicit safearray_t(args_t&&... args) WI_NOEXCEPT : storage_t(wistd::forward<args_t>(args)...) {}

        // Exception-based construction
        template<typename T = element_t, typename wistd::enable_if<is_type_not_set<T>::value, int>::type = 0>
        safearray_t(VARTYPE vt, UINT cElements, LONG lowerBound = 0)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create(vt, cElements, lowerBound);
        }
        template<typename T, typename U = element_t, typename wistd::enable_if<is_type_not_set<U>::value, int>::type = 0>
        safearray_t(UINT cElements, LONG lowerBound = 0)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create<T>(cElements, lowerBound);
        }
        template<typename T = element_t, typename wistd::enable_if<is_valid_type<T>::value, int>::type = 0>
        safearray_t(UINT cElements, LONG lowerBound = 0)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create(cElements, lowerBound);
        }

        // Low-level, arbitrary number of dimensions
        template<typename T = element_t, typename wistd::enable_if<is_type_not_set<T>::value, int>::type = 0>
        result create(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* sab)
        {
            return err_policy::HResult(_create(vt, cDims, sab));
        }

        // Single Dimension specialization
        template<typename T = element_t, typename wistd::enable_if<is_type_not_set<T>::value, int>::type = 0>
        result create(VARTYPE vt, UINT cElements, LONG lowerBound = 0)
        {
            auto bounds = SAFEARRAYBOUND{ cElements, lowerBound };
            return err_policy::HResult(_create(vt, 1, &bounds));
        }

        // Uses the fixed element type defined by the class
        // Low-level, arbitrary number of dimensions
        template<typename T = element_t, typename wistd::enable_if<is_valid_type<T>::value, int>::type = 0>
        result create(UINT cDims, SAFEARRAYBOUND* sab)
        {
            constexpr auto vt = static_cast<VARTYPE>(details::VarTraits<T>::type);
            return err_policy::HResult(_create(vt, cDims, sab));
        }

        // Single Dimension specialization
        template<typename T = element_t, typename wistd::enable_if<is_valid_type<T>::value, int>::type = 0>
        result create(UINT cElements, LONG lowerBound = 0)
        {
            constexpr auto vt = static_cast<VARTYPE>(details::VarTraits<T>::type);
            auto bounds = SAFEARRAYBOUND{ cElements, lowerBound };
            return err_policy::HResult(_create(vt, 1, &bounds));
        }

        // Create a Copy
        result create_copy(pointer psaSrc)
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

        // Lowest-Level functions for those who know what they're doing
            // TODO: Finish safearray_index type to collapse both versions into one functioon
        result put_element(LONG* pIndices, void* pv)
        {
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), pIndices, pv));
        }
        result put_element(LONG nIndex, void* pv)
        {
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), &nIndex, pv));
        }
        result get_element(LONG* pIndices, void* pv)
        {
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), pIndices, pv));
        }
        result get_element(LONG nIndex, void* pv)
        {
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), &nIndex, pv));
        }

        template<typename T = element_t, typename wistd::enable_if<is_value_type<T>::value, int>::type = 0>
        result put_element(LONG nIndex, const T& val)
        {
            WI_ASSERT(sizeof(val) == elemsize());
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), &nIndex, reinterpret_cast<void*>(const_cast<T*>(&val))));
        }
        template<typename T = element_t, typename wistd::enable_if<is_pointer_type<T>::value, int>::type = 0>
        result put_element(LONG nIndex, T val)
        {
            WI_ASSERT(sizeof(val) == elemsize());
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), &nIndex, val));
        }

        template<typename T = element_t, typename wistd::enable_if<is_valid_type<T>::value, int>::type = 0>
        result get_element(LONG nIndex, T& t)
        {
            WI_ASSERT(sizeof(t) == elemsize());
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), &nIndex, &t));
        }
        //template<typename T> using EnableIfTyped = typename wistd::enable_if<!wistd::is_same<T, void>::value, int>::type;
        //// Filters functions that require a type because the class type doesn't provide one (element_t == void)
        //template<typename T> using EnableIfNotTyped = typename wistd::enable_if<wistd::is_same<T, void>::value, int>::type;

        //template<typename T = element_t, EnableIfTyped<T> = 0>
        //result get_element(LONG nIndex, T& t)
        //{
        //    WI_ASSERT(sizeof(t) == elemsize());
        //    WI_ASSERT(dim() == 1);
        //    return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), &nIndex, &t));
        //}

        // Data Access
            // Use with non-typed safearrays after the type has been determined
        //template<typename T, typename U = element_t, EnableIfNotTyped<U> = 0>
        //result access_data(unique_accessdata_t<T>& data)
        //{
        //    return err_policy::HResult(data.create(storage_t::get()));
        //}
        //    // Use with type safearrays
        //template<typename T = element_t, EnableIfTyped<T> = 0>
        //result access_data(unique_accessdata& data)
        //{
        //    return err_policy::HResult(data.create(storage_t::get()));
        //}

        // Exception-based helper functions
        WI_NODISCARD LONG lbound(UINT nDim = 1) const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            LONG nResult = 0;
            lbound(nDim, &nResult);
            return nResult;
        }
        WI_NODISCARD LONG ubound(UINT nDim = 1) const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            LONG nResult = 0;
            ubound(nDim, &nResult);
            return nResult;
        }
        WI_NODISCARD ULONG count() const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            ULONG nResult = 0;
            count(&nResult);
            return nResult;
        }
        WI_NODISCARD unique_type create_copy() const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");

            auto result = unique_type{};
            result.create_copy(storage_t::get());
            return result;
        }
        template<typename T, typename U = element_t, typename wistd::enable_if<is_type_not_set<U>::value, int>::type = 0>
        WI_NODISCARD unique_accessdata_t<T> access_data() const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");

            auto data = unique_accessdata_t<T>{};
            data.create(storage_t::get());
            return wistd::move(data);
        }
        template<typename T = element_t, typename wistd::enable_if<is_valid_type<T>::value, int>::type = 0>
        WI_NODISCARD unique_accessdata access_data() const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");

            auto data = unique_accessdata{};
            data.create(storage_t::get());
            return wistd::move(data);
        }

    private:
        HRESULT _create(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* sab)
        {
            auto psa = storage_t::policy::invalid_value();
            RETURN_IF_FAILED(details::SafeArrayCreate(vt, cDims, sab, psa));
            storage_t::reset(psa);
            return S_OK;
        }
    };

    using unique_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, void>>;
    using unique_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, void>>;
    using unique_char_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, char>>;
    using unique_char_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, char>>;
    using unique_short_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, short>>;
    using unique_short_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, short>>;
    using unique_long_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, long>>;
    using unique_long_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, long>>;
    using unique_int_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, int>>;
    using unique_int_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, int>>;
    using unique_longlong_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, long long>>;
    using unique_longlong_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, long long>>;
    using unique_byte_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, byte>>;
    using unique_byte_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, byte>>;
    using unique_word_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, WORD>>;
    using unique_word_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, WORD>>;
    using unique_dword_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, DWORD>>;
    using unique_dword_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, DWORD>>;
    using unique_ulonglong_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, ULONGLONG>>;
    using unique_ulonglong_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, ULONGLONG>>;
    using unique_float_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, float>>;
    using unique_float_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, float>>;
    using unique_double_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, double>>;
    using unique_double_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, double>>;
    using unique_varbool_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, VARIANT_BOOL>>;
    using unique_varbool_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, VARIANT_BOOL>>;
    using unique_date_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, DATE>>;
    using unique_date_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, DATE>>;
    using unique_currency_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, CURRENCY>>;
    using unique_currency_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, CURRENCY>>;
    using unique_decimal_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, DECIMAL>>;
    using unique_decimal_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, DECIMAL>>;
    using unique_bstr_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, BSTR>>;
    using unique_bstr_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, BSTR>>;
    using unique_unknown_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, LPUNKNOWN>>;
    using unique_unknown_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, LPUNKNOWN>>;
    using unique_dispatch_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, LPDISPATCH>>;
    using unique_dispatch_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, LPDISPATCH>>;
    using unique_variant_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, VARIANT>>;
    using unique_variant_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, VARIANT>>;

#ifdef WIL_ENABLE_EXCEPTIONS
    using unique_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, void>>;
    using unique_char_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, char>>;
    using unique_short_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, short>>;
    using unique_long_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, long>>;
    using unique_int_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, int>>;
    using unique_longlong_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, long long>>;
    using unique_byte_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, byte>>;
    using unique_word_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, WORD>>;
    using unique_dword_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, DWORD>>;
    using unique_ulonglong_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, ULONGLONG>>;
    using unique_float_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, float>>;
    using unique_double_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, double>>;
    using unique_varbool_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, VARIANT_BOOL>>;
    using unique_date_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, DATE>>;
    using unique_currency_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, CURRENCY>>;
    using unique_decimal_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, DECIMAL>>;
    using unique_bstr_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, BSTR>>;
    using unique_unknown_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, LPUNKNOWN>>;
    using unique_dispatch_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, LPDISPATCH>>;
    using unique_variant_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, VARIANT>>;
#endif

#endif

#ifdef WIL_ENABLE_EXCEPTIONS
#endif // WIL_ENABLE_EXCEPTIONS
} // namespace wil

#endif

