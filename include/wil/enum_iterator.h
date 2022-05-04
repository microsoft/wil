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
#ifndef __WIL_ENUM_ITERATOR
#define __WIL_ENUM_ITERATOR

#include "com.h"
#include "result_macros.h"

#include <memory>
#include "resource.h"
#include <type_traits>
#include <tuple>

#include <type_traits>
#include <tuple>


#if !defined(WIL_RESOURCE_STL) || (__WI_LIBCPP_STD_VER < 17)
#error Needs STL and C++17
#endif

namespace wil
{
    template<typename T>
    struct shared_cotaskmem_ptr : std::shared_ptr<T>
    {
        shared_cotaskmem_ptr() : std::shared_ptr<T>() {}
        shared_cotaskmem_ptr(T* t) : std::shared_ptr<T>(t, wil::cotaskmem_deleter{}) {}
    };

    template<typename T, typename... Args>
    auto make_shared_cotaskmem_ptr(Args&&... args)
    {
        static_assert(wistd::is_trivially_destructible<T>::value, "T has a destructor that won't be run when used with this function; use make_unique instead");
        shared_cotaskmem_ptr<T> sp(static_cast<T*>(::CoTaskMemAlloc(sizeof(T))));
        if (sp)
        {
            // use placement new to initialize memory from the previous allocation
            new (sp.get()) T(wistd::forward<Args>(args)...);
        }
        return sp;
    }

    namespace Details
    {
        template<typename R, typename T, typename... Args>
        struct FunctionTraitsBase
        {
            using RetType = R;
            using Type = T;
            using ArgTypes = std::tuple<Args...>;
            static constexpr std::size_t ArgCount = sizeof...(Args);
            template<std::size_t N>
            using NthArg = std::tuple_element_t<N, ArgTypes>;
        };

        template<typename F> struct FunctionTraits;

        template<typename T, typename Arg>
        struct FunctionTraits<HRESULT(__stdcall T::*)(ULONG, Arg**, ULONG*)>
            : FunctionTraitsBase<HRESULT, T, ULONG, Arg, ULONG*>
        {};

        template<typename T, typename U = void>
        struct has_AddRef_helper : std::false_type {};

        template<typename T> struct has_AddRef_helper<T, std::void_t<decltype(&T::AddRef)>> : std::true_type {};

        template<typename T> constexpr bool has_AddRef_v = has_AddRef_helper<T>::value;

        template<typename T, typename U = void>
        struct SafeWrapper
        {
            using type = T;
        };

        template<typename T>
        struct SafeWrapper<T, std::enable_if_t<has_AddRef_v<T>>>
        {
            using type = wil::com_ptr<T>;
        };

        template<typename T>
        using safe_wrapper_t = typename SafeWrapper<T>::type;

#ifdef __cpp_noexcept_function_type
        template<typename T, typename Arg>
        struct FunctionTraits<HRESULT(__stdcall T::*)(ULONG, Arg**, ULONG*) noexcept>
            : FunctionTraitsBase<HRESULT, T, ULONG, Arg, ULONG*>
        {};
#endif

        template<typename TEnum>
        using NextArgType = typename Details::FunctionTraits<decltype(&TEnum::Next)>::template NthArg<1>;
    }

    template<typename TEnum>
    /// <summary>
    /// Iterator class for iterating through types that implement the IEnum* COM pattern.
    /// </summary>
    /// <typeparam name="TEnum">The enumerator type, e.g. IEnumIDList, IDiaEnumSymbols.</typeparam>
    struct enum_iterator
    {
        enum_iterator(const enum_iterator& other) = default;
        enum_iterator(enum_iterator&&) = default;
        enum_iterator(TEnum* pEnum) : m_pEnum(pEnum)
        {
            (*this)++;
        }
        /// <summary>
        /// The type of the out parameter in Next(...) that corresponds to the element type being enumerated.
        /// e.g. PITEMID_CHILD, IDiaSymbol, etc.
        /// </summary>
        using TElemRaw = Details::NextArgType<TEnum>;
        using TElem = Details::safe_wrapper_t<TElemRaw>;

        static constexpr bool is_cotaskmem = std::is_same_v<TElem, wil::shared_cotaskmem>;
        static constexpr bool is_cotaskmem_ptr = std::is_same_v<TElem, wil::shared_cotaskmem_ptr<TElemRaw>>;
        static constexpr bool is_cotaskmem_or_ptr = is_cotaskmem || is_cotaskmem_ptr;

        ~enum_iterator() = default;
    private:
        
        HRESULT CallNextAndAssign(ULONG* celt)
        {
            if constexpr (!is_cotaskmem_or_ptr)
            {
                return m_pEnum->Next(1, &m_current, celt);
            }
            else if constexpr (is_cotaskmem)
            {
                return m_pEnum->Next(1, m_current.put(), celt);
            }
            else if constexpr (is_cotaskmem_ptr)
            {
                TElemRaw* rawPtr{ nullptr };
                auto hr = m_pEnum->Next(1, &rawPtr, celt);
                m_current.reset(rawPtr, wil::cotaskmem_deleter{});
                return hr;
            }
        }


    public:
        auto& operator++(int) { return this->operator++(); }
        auto& operator++()
        {
            TElem pChild{ };
            ULONG celt = 0;
            HRESULT hr = CallNextAndAssign(&celt);

            if ((hr == S_OK) && (celt == 1))
            {
                // done
            }
            else if (hr == S_FALSE)
            {
                m_end = true;
            }
            else
            {
                THROW_HR(hr);
            }
            return *this;
        }

        TElem const& operator*() const noexcept { return m_current; }
        TElem const& operator->() const noexcept { return m_current; }

        auto operator+(int v)
        {
            auto other = *this;
            for (int i = 0; i < v; i++)
            {
                other++;
            }
            return other;
        }

        bool operator==(const enum_iterator& o) const noexcept
        {
            if (m_end ^ o.m_end) return false;
            if ((m_end == o.m_end) && m_end) return true;
            return (o.m_current == this->m_current) && (o.m_pEnum == m_pEnum) && (m_end == o.m_end);
        }

        bool operator!=(const enum_iterator& o) const noexcept { return !(*this == o); }

        static constexpr enum_iterator end() noexcept
        {
            return { end_tag{} };
        }
    protected:
        struct end_tag {};
        enum_iterator(end_tag) : m_end(true) {}
        enum_iterator() {};
        wil::com_ptr<TEnum> m_pEnum{ nullptr };
        TElem m_current{ nullptr };
        bool m_end{ false };
    };
}

namespace wistd
{
    template<typename TEnum>
    wil::enum_iterator<TEnum> begin(TEnum* pEnum)
    {
        return { pEnum };
    }
    template<typename TEnum>
    constexpr wil::enum_iterator<TEnum> end(TEnum*)
    {
        return wil::enum_iterator<TEnum>::end();
    }
}

#endif