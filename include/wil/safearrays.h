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
        struct var_traits{};

        template<> struct var_traits<char>                       { static constexpr auto vartype = VT_I1; };
        //template<> struct var_traits<short>                      { static constexpr auto vartype = VT_I2; };
        template<> struct var_traits<long>                       { static constexpr auto vartype = VT_I4; };
        template<> struct var_traits<int>                        { static constexpr auto vartype = VT_I4; };
        template<> struct var_traits<long long>                  { static constexpr auto vartype = VT_I8; };
        template<> struct var_traits<unsigned char>              { static constexpr auto vartype = VT_UI1; };
        template<> struct var_traits<unsigned short>             { static constexpr auto vartype = VT_UI2; };
        template<> struct var_traits<unsigned long>              { static constexpr auto vartype = VT_UI4; };
        template<> struct var_traits<unsigned int>               { static constexpr auto vartype = VT_UI4; };
        template<> struct var_traits<unsigned long long>         { static constexpr auto vartype = VT_UI8; };
        template<> struct var_traits<float>                      { static constexpr auto vartype = VT_R4; };
        //template<> struct var_traits<double>                     { static constexpr auto vartype = VT_R8; };
        template<> struct var_traits<VARIANT_BOOL>               { static constexpr auto vartype = VT_BOOL; };
        template<> struct var_traits<DATE>                       { static constexpr auto vartype = VT_DATE; };
        template<> struct var_traits<CURRENCY>                   { static constexpr auto vartype = VT_CY; };
        template<> struct var_traits<DECIMAL>                    { static constexpr auto vartype = VT_DECIMAL; };
        template<> struct var_traits<BSTR>                       { static constexpr auto vartype = VT_BSTR; };
        template<> struct var_traits<LPUNKNOWN>                  { static constexpr auto vartype = VT_UNKNOWN; };
        template<> struct var_traits<LPDISPATCH>                 { static constexpr auto vartype = VT_DISPATCH; };
        template<> struct var_traits<VARIANT>                    { static constexpr auto vartype = VT_VARIANT; };

        inline void __stdcall SafeArrayDestroy(SAFEARRAY* psa) WI_NOEXCEPT
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
            VARTYPE vt = VT_NULL;   // Invalid for SAs, so use to mean SA was null
            if ((psa != nullptr) && FAILED(::SafeArrayGetVartype(psa, &vt)))
            {
                vt = VT_EMPTY;  // Invalid for SAs, use to mean type couldn't be determined
            }
            return vt;
        }

        inline ULONG __stdcall SafeArrayGetLockCount(SAFEARRAY* psa) WI_NOEXCEPT
        {
            return (psa != nullptr) ? psa->cLocks : 0U;
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

        inline HRESULT __stdcall SafeArrayDimElementCount(SAFEARRAY* psa, UINT nDim, ULONG* pCount) WI_NOEXCEPT
        {
            __FAIL_FAST_ASSERT__(psa != nullptr);
            __FAIL_FAST_ASSERT__(pCount != nullptr);
            RETURN_HR_IF(E_INVALIDARG, ((nDim == 0) || (nDim > psa->cDims)));
            // Read from the back of the array because SAFEARRAYs are stored with the dimensions in reverse order
            *pCount = psa->rgsabound[psa->cDims - nDim].cElements;
            return S_OK;
        }

        inline HRESULT __stdcall SafeArrayCountElements(SAFEARRAY* psa, ULONG* pCount) WI_NOEXCEPT
        {
            __FAIL_FAST_ASSERT__(pCount != nullptr);
            if (psa != nullptr)
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

        typedef resource_policy<SAFEARRAY*, decltype(&details::SafeArrayDestroy), details::SafeArrayDestroy, details::pointer_access_all> safearray_resource_policy;
    }
    /// @endcond

    typedef unique_any<SAFEARRAY*, decltype(&details::SafeArrayUnlock), details::SafeArrayUnlock, details::pointer_access_noaddress> safearray_unlock_scope_exit;

    //! Guarantees a SafeArrayUnlock call on the given object when the returned object goes out of scope
    //! Note: call SafeArrayUnlock early with the reset() method on the returned object or abort the call with the release() method
    WI_NODISCARD inline safearray_unlock_scope_exit SafeArrayUnlock_scope_exit(SAFEARRAY* psa) WI_NOEXCEPT
    {
        details::SafeArrayLock(psa);
        return safearray_unlock_scope_exit(psa);
    }

    //! Class that facilitates the direct access to the contents of a SAFEARRAY and "unaccessing" it when done
    //! It enables treating the safearray like an regular array, or doing a ranged-for on the contents.
    //! Accessing a safearray increases it's lock count, so attempts to destroy the safearray will fail until it is unaccessed.
    //! NOTE: This class does not manage the lifetime of the SAFEARRAY itself.  See @ref safearray_t.
    //! ~~~~
    //! HRESULT copy_to_bstr_vector(SAFEARRAY* psa, std::vector<wil::unique_bstr>& result)
    //! {
    //!     auto data = wil::safearraydata_nothrow<BSTR>{};
    //!     RETURN_IF_FAILED(data.access(psa));
    //!     result.reserve(result.size() + data.size());
    //!     for(BSTR& bstr : data)
    //!     {
    //!         result.emplace_back(bstr);
    //!     }
    //!     return S_OK;
    //! }
    //! ~~~~
    //! unique_bstr_safearray move_to_safearray(std::vector<wil::unique_bstr>& source)
    //! {
    //!     auto sa = wil::unique_bstr_safearray{source.size()};
    //!     auto current = std::begin(source);
    //!     for(BSTR& bstr : sa.access_data())
    //!     {
    //!         // Transfer ownership of the BSTR into the safearray
    //!         bstr = (*current++).release();
    //!     }
    //!     return sa;
    //! }
    //! ~~~~
    //! void handle_safearray_of_bools(SAFEARRAY* psa, std::function<void(bool, ULONG)> fnHandler)
    //! {
    //!     auto data = wil::safearraydata<VARIANT_BOOL>{psa};
    //!     for (auto i = 0U; i < data.size(); ++i)
    //!     {
    //!         fnHandler(data[i] != VARIANT_FALSE, i);
    //!     }
    //! }
    //! ~~~~
    //! template<typename T>
    //! std::vector<T> extract_all_values(SAFEARRAY* psa)
    //! {
    //!     auto data = wil::safearraydata<T>{psa};
    //!     // Construct the vector from the iterators
    //!     return {data.begin(), data.end()};
    //! }
    //! ~~~~
    //! template<typename INTERFACE>
    //! std::vector<wil::com_ptr<T>> extract_all_interfaces(SAFEARRAY* psa)
    //! {
    //!     auto result = std::vector<wil::com_ptr<INTERFACE>>{};
    //!     auto data = wil::safearraydata<LPUNKNOWN>{psa};
    //!     result.reserve(data.size());
    //!     for(auto& p : data) // type of P is LPUNKNOWN
    //!     {
    //!         // Use "tag_com_query" if you want failure instead of nullptr if INTERFACE not supported
    //!         result.emplace_back(p, details::tag_try_com_query);
    //!     }
    //!     return result;
    //! }
    //! ~~~~
    //! @tparam T           The underlying datatype expected in the SAFEARRAY. It should be a scalar type, BSTR, VARIANT_BOOL, LPUNKNOWN/LPDISPATCH, etc.
    //! @tparam err_policy  Represents the error policy for the class (error codes, exceptions, or fail fast; see @ref page_errors)
    template <typename T, typename err_policy = err_exception_policy>
    class safearraydata_t
    {
    private:
        typedef unique_any<SAFEARRAY*, decltype(&details::SafeArrayUnaccessData), details::SafeArrayUnaccessData, details::pointer_access_noaddress> safearray_unaccess_data;

    public:
        typedef typename err_policy::result result;
        typedef typename safearray_unaccess_data::pointer pointer;
        typedef T value_type;
        typedef value_type* value_pointer;
        typedef const value_type* const_value_pointer;
        typedef value_type& value_reference;
        typedef const value_type& const_value_reference;
        typedef value_type* iterator;
        typedef const value_type* const_iterator;

    public:
        // Exception-based construction
        safearraydata_t(pointer psa)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            access(psa);
        }
        safearraydata_t() = default;
        safearraydata_t(safearraydata_t&&) = default;
        ~safearraydata_t() = default;
        safearraydata_t& operator=(safearraydata_t&&) = default;
        safearraydata_t(const safearraydata_t&) = delete;
        safearraydata_t& operator=(const safearraydata_t&) = delete;

        result access(pointer psa)
        {
            HRESULT hr = [&]()
            {
                WI_ASSERT(sizeof(T) == ::SafeArrayGetElemsize(psa));
                details::SafeArrayAccessData(psa, m_pBegin);
                m_unaccess.reset(psa);
                RETURN_IF_FAILED(details::SafeArrayCountElements(m_unaccess.get(), &m_nSize));
                return S_OK;
            }();

            return err_policy::HResult(hr);
        }

        // Ranged-for style
        iterator begin() WI_NOEXCEPT { WI_ASSERT(m_pBegin != nullptr); return m_pBegin; }
        iterator end() WI_NOEXCEPT { WI_ASSERT(m_pBegin != nullptr); return m_pBegin+m_nSize; }
        const_iterator begin() const WI_NOEXCEPT { WI_ASSERT(m_pBegin != nullptr); return m_pBegin; }
        const_iterator end() const WI_NOEXCEPT { WI_ASSERT(m_pBegin != nullptr); return m_pBegin+m_nSize; }

        // Old style iteration
        ULONG size() const WI_NOEXCEPT { return m_nSize; }
        WI_NODISCARD value_reference operator[](ULONG i) { WI_ASSERT(i < m_nSize); return *(m_pBegin + i); }
        WI_NODISCARD const_value_reference operator[](ULONG i) const { WI_ASSERT(i < m_nSize); return *(m_pBegin +i); }

        // Utilities
        bool empty() const WI_NOEXCEPT { return m_nSize == ULONG{}; }

    private:
        safearray_unaccess_data m_unaccess{};
        value_pointer m_pBegin{};
        ULONG m_nSize{};
    };

    template<typename T>
    using safearraydata_nothrow = safearraydata_t<T, err_returncode_policy>;
    template<typename T>
    using safearraydata_failfast = safearraydata_t<T, err_failfast_policy>;

#ifdef WIL_ENABLE_EXCEPTIONS
    template<typename T>
    using safearraydata = safearraydata_t<T, err_exception_policy>;
#endif

    //! RAII class for SAFEARRAYs that lives between unique_any_t and unique_storage to provide extra SAFEARRAY functionality.  
    //! SAFEARRAYs provide a convenient way of passing an array of values across APIs and can be useful because they will clean 
    //! up their resources (interface ref counts, BSTRs, etc.) when properly destroyed.  If you have a SAFEARRAY of interface 
    //! pointers or BSTRs, there is no need to call Release or SysFreeString on each element (that is done automatically
    //! when the array is destroyed in SafeArrayDestroy), so the only resource that needs to be managed is the SAFEARRAY
    //! itself.
    //! ~~~~
    //! // Return a SAFEARRAY from an API
    //! HRESULT GetWonderfulData(SAFEARRAY** ppsa)
    //! {
    //!     wil::unique_bstr_safearray_nothrow  sa{};
    //!     RETURN_IF_FAILED(sa.create(32));
    //!     {
    //!         wil::safearraydata_nothrow<BSTR>    data{};
    //!         RETURN_IF_FAILED(data.access(sa.get());
    //!         for(auto& bstr : data)
    //!         {
    //!             // SAFEARRAY will own this string and clean it up
    //!             bstr = ::SysAllocString(L"Wonderful!");
    //!             // Even if we bail early, nothing will leak
    //!             RETURN_HR_IF_NULL(E_OUTOFMEMORY, bstr);
    //!         }
    //!     }
    //!     *ppsa = sa.release();
    //!     return S_OK;
    //! }
    //! ~~~~
    //! // Obtain a SAFEARRAY from an API
    //! HRESULT ProcessWonderful()
    //! {
    //!     wil::unique_safearray sa{};
    //!
    //!     // Invoke the API
    //!     RETURN_IF_FAILED(::GetWonderfulData(sa.put()));
    //!     // Verify the output is expected
    //!     RETURN_HR_IF(E_UNEXPECTED, sa.vartype() != VT_BSTR);
    //!     for(auto& bstr : sa.access_data<BSTR>())
    //!     {
    //!         // Use the BSTR, no clean up necessary
    //!         DoSomethingWithBSTR(bstr);
    //!     }
    //!     return S_OK;
    //! }
    //! ~~~~
    //! @tparam storage_t   Storage class that manages the SAFEARRAY pointer
    //! @tparam err_policy  Represents the error policy for the class (error codes, exceptions, or fail fast; see @ref page_errors)
    //! @tparam element_t   The underlying datatype that is contained in the safearray OR
    //!                     it can be void to make the safearray generic (and not as typesafe)
    template <typename storage_t, typename err_policy = err_exception_policy, typename element_t = void>
    class safearray_t : public storage_t
    {
    private:
        // SFINAE Helpers to improve readability
        template<typename T>
        using is_pointer_type = wistd::disjunction<wistd::is_same<T, BSTR>,
                                                    wistd::is_same<T, LPUNKNOWN>,
                                                    wistd::is_same<T, LPDISPATCH>>;
        template<typename T>
        using is_type_not_set = wistd::is_same<T, void>;
        template<typename T>
        using is_type_set = wistd::negation<is_type_not_set<T>>;
        template <typename T>
        using is_value_type = wistd::conjunction<wistd::negation<is_pointer_type<T>>, is_type_set<T>>;

    public:
        template<typename T>
        using safearraydata_t = safearraydata_t<T, err_policy>;

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
        template<typename T = element_t, typename wistd::enable_if<is_type_set<T>::value, int>::type = 0>
        safearray_t(UINT cElements, LONG lowerBound = 0)
        {
            static_assert(wistd::is_same<void, result>::value, "this constructor requires exceptions; use the create method");
            create<T>(cElements, lowerBound);
        }

        // Low-level, arbitrary number of dimensions
        // Uses Explicit Type
        template<typename T = element_t, typename wistd::enable_if<is_type_not_set<T>::value, int>::type = 0>
        result create(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* sab)
        {
            return err_policy::HResult(_create(vt, cDims, sab));
        }
        // Uses Inferred Type
        template<typename T = element_t, typename wistd::enable_if<is_type_set<T>::value, int>::type = 0>
        result create(UINT cDims, SAFEARRAYBOUND* sab)
        {
            constexpr auto vt = static_cast<VARTYPE>(details::var_traits<T>::vartype);
            return err_policy::HResult(_create(vt, cDims, sab));
        }

        // Single Dimension Specialization
        // Uses Explicit Type
        template<typename T = element_t, typename wistd::enable_if<is_type_not_set<T>::value, int>::type = 0>
        result create(VARTYPE vt, UINT cElements, LONG lowerBound = 0)
        {
            auto bounds = SAFEARRAYBOUND{ cElements, lowerBound };
            return err_policy::HResult(_create(vt, 1, &bounds));
        }
        // Uses Inferred Type
        template<typename T = element_t, typename wistd::enable_if<is_type_set<T>::value, int>::type = 0>
        result create(UINT cElements, LONG lowerBound = 0)
        {
            constexpr auto vt = static_cast<VARTYPE>(details::var_traits<T>::vartype);
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
            WI_ASSERT(pLbound != nullptr);
            WI_ASSERT((nDim > 0) && (nDim <= dim()));
            return err_policy::HResult(::SafeArrayGetLBound(storage_t::get(), nDim, pLbound));
        }
        result lbound(LONG* pLbound) const
        {
            WI_ASSERT(dim() == 1);
            WI_ASSERT(pLbound != nullptr);
            return err_policy::HResult(::SafeArrayGetLBound(storage_t::get(), 1, pLbound));
        }
        result ubound(UINT nDim, LONG* pUbound) const
        {
            WI_ASSERT(pUbound != nullptr);
            WI_ASSERT((nDim > 0) && (nDim <= dim()));
            return err_policy::HResult(::SafeArrayGetUBound(storage_t::get(), nDim, pUbound));
        }
        result ubound(LONG* pUbound) const
        {
            WI_ASSERT(dim() == 1);
            WI_ASSERT(pUbound != nullptr);
            return err_policy::HResult(::SafeArrayGetUBound(storage_t::get(), 1, pUbound));
        }
        result count(ULONG* pCount) const
        {
            WI_ASSERT(pCount != nullptr);
            return err_policy::HResult(details::SafeArrayCountElements(storage_t::get(), pCount));
        }
        // Same as, but much faster than, the typical UBOUND - LBOUND + 1
        result dim_elements(UINT nDim, ULONG* pCount) const
        {
            WI_ASSERT(pCount != nullptr);
            WI_ASSERT((nDim > 0) && (nDim <= dim()));
            return err_policy::HResult(details::SafeArrayDimElementCount(storage_t::get(), nDim, pCount));
        }

        //! Retrieves the stored type (when not working with a fixed type)
        template<typename T = element_t, typename wistd::enable_if<is_type_not_set<T>::value, int>::type = 0>
        VARTYPE vartype()
        {
            return details::SafeArrayGetVartype(storage_t::get());
        }

        //! Returns the current number of locks on the SAFEARRAY
        ULONG lock_count() const
        {
            return details::SafeArrayGetLockCount(storage_t::get());
        }

        //! Returns an object that keeps a lock count on the safearray until it goes out of scope
        //! Use to keep the SAFEARRAY from being destroyed but without access the contents
        //! Not needed when using safearraydata_t because accessing the data also holds a lock
        //! ~~~~
        //! void Process(wil::unique_safearray& sa)
        //! {
        //!     auto lock = sa.scope_lock();
        //!     LongRunningFunction(sa);
        //! }
        WI_NODISCARD safearray_unlock_scope_exit scope_lock() const WI_NOEXCEPT
        {
            return wil::SafeArrayUnlock_scope_exit(storage_t::get());
        }


        //! Element Access
        //! Instead of using the safearraydata_t to access the entire collection of elements
        //! at once, it is also possible to access them one at a time.  However, using this
        //! functionality is less efficient because it deals with copies of the elements
        //! that the caller is then responsible for cleaning up, so use RAII data types
        //! for items that require clean up.
        //! ~~~~
        //! HRESULT ProcessWonderful_UsingElements()
        //! {
        //!     wil::unique_safearray_nothrow sa{};
        //!     wil::unique_bstr bstr{};
        //!     ULONG count{};
        //!
        //!     // Invoke the API
        //!     RETURN_IF_FAILED(::GetWonderfulData(sa.put()));
        //!     // Verify the output is expected
        //!     RETURN_HR_IF(E_UNEXPECTED, sa.vartype() != VT_BSTR);
        //!     RETURN_HR_IF(E_UNEXPECTED, sa.dims() != 1);
        //!     RETURN_IF_FAILED(sa.count(&count));
        //!     for(auto i = 0U; i < count; ++i)
        //!     {
        //!         // Copy from the safearray to the unique_bstr
        //!         sa.get_element(i, bstr.put());
        //!         DoSomethingWithBSTR(bstr);
        //!     }
        //!     return S_OK;
        //! }
        //! ~~~~
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
        template<typename T = element_t, typename wistd::enable_if<is_type_set<T>::value, int>::type = 0>
        result get_element(LONG nIndex, T& t)
        {
            WI_ASSERT(sizeof(t) == elemsize());
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), &nIndex, &t));
        }

        // Lowest-Level functions for those who know what they're doing
        result put_element(LONG nIndex, void* pv)
        {
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), &nIndex, pv));
        }
        result put_element(LONG* pIndices, void* pv)
        {
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), pIndices, pv));
        }
        result get_element(LONG nIndex, void* pv)
        {
            WI_ASSERT(dim() == 1);
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), &nIndex, pv));
        }
        result get_element(LONG* pIndices, void* pv)
        {
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), pIndices, pv));
        }

        // Multi Dimension Support
        template<typename T = element_t, typename wistd::enable_if<is_value_type<T>::value, int>::type = 0>
        result put_element(LONG* pIndices, const T& val)
        {
            WI_ASSERT(sizeof(val) == elemsize());
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), pIndices, reinterpret_cast<void*>(const_cast<T*>(&val))));
        }
        template<typename T = element_t, typename wistd::enable_if<is_pointer_type<T>::value, int>::type = 0>
        result put_element(LONG* pIndices, T val)
        {
            WI_ASSERT(sizeof(val) == elemsize());
            return err_policy::HResult(::SafeArrayPutElement(storage_t::get(), pIndices, val));
        }
        template<typename T = element_t, typename wistd::enable_if<is_type_set<T>::value, int>::type = 0>
        result get_element(LONG* pIndices, T& t)
        {
            WI_ASSERT(sizeof(t) == elemsize());
            return err_policy::HResult(::SafeArrayGetElement(storage_t::get(), pIndices, &t));
        }

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
        WI_NODISCARD ULONG dim_elements(UINT nDim = 1) const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            ULONG nResult = 0;
            dim_elements(nDim, &nResult);
            return nResult;
        }
        WI_NODISCARD unique_type create_copy() const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");

            auto result = unique_type{};
            result.create_copy(storage_t::get());
            return result;
        }
        //! Returns a safearraydata_t that provides direct access to this SAFEARRAYs contents
        //! ~~~~
        //! unique_bstr_safearray copy_to_safearray(std::vector<wil::unique_bstr>& source)
        //! {
        //!     auto sa = wil::unique_bstr_safearray{source.size()};
        //!     auto current = std::begin(source);
        //!     for(BSTR& bstr : sa.access_data())
        //!     {
        //!         // Create a copy for the safearray to own
        //!         bstr = ::SysAllocString((*current++).get());
        //!     }
        //!     return sa;
        //! }
        //! ~~~~
        template<typename T = element_t, typename wistd::enable_if<is_type_set<T>::value, int>::type = 0>
        WI_NODISCARD safearraydata_t<T> access_data() const
        {
            static_assert(wistd::is_same<void, result>::value, "this method requires exceptions");
            WI_ASSERT(static_cast<VARTYPE>(details::var_traits<T>::vartype) == details::SafeArrayGetVartype(storage_t::get()));

            auto data = safearraydata_t<T>{};
            data.access(storage_t::get());
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
    //using unique_short_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, short>>;
    //using unique_short_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, short>>;
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
    //using unique_double_safearray_nothrow = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_returncode_policy, double>>;
    //using unique_double_safearray_failfast = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_failfast_policy, double>>;
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
    //using unique_short_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, short>>;
    using unique_long_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, long>>;
    using unique_int_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, int>>;
    using unique_longlong_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, long long>>;
    using unique_byte_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, byte>>;
    using unique_word_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, WORD>>;
    using unique_dword_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, DWORD>>;
    using unique_ulonglong_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, ULONGLONG>>;
    using unique_float_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, float>>;
    //using unique_double_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, double>>;
    using unique_varbool_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, VARIANT_BOOL>>;
    using unique_date_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, DATE>>;
    using unique_currency_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, CURRENCY>>;
    using unique_decimal_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, DECIMAL>>;
    using unique_bstr_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, BSTR>>;
    using unique_unknown_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, LPUNKNOWN>>;
    using unique_dispatch_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, LPDISPATCH>>;
    using unique_variant_safearray = unique_any_t<safearray_t<details::unique_storage<details::safearray_resource_policy>, err_exception_policy, VARIANT>>;
#endif


#endif  // #if defined( _OLEAUTO_H_ ) && !defined(__WIL_OLEAUTO_)
} // namespace wil

#endif  // #ifndef __WIL_SAFEARRAYS_INCLUDED

