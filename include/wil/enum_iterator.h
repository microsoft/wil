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
#include <type_traits>
#include <tuple>


#include <type_traits>
#include <tuple>
#include <cstdio>

namespace wil {

    namespace Details {
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

        template<typename TEnum>
        using NextArgType = typename Details::FunctionTraits<decltype(&TEnum::template Next)>::template NthArg<1>;

        
    }

    template<typename TEnum>
    /// <summary>
    /// Iterator class for iterating through types that implement the IEnum* COM pattern.
    /// </summary>
    /// <typeparam name="TEnum">The enumerator type, e.g. IEnumIDList, IDiaEnumSymbols.</typeparam>
    struct enum_iterator{
        enum_iterator(const enum_iterator& other) = default;
        enum_iterator(TEnum* pEnum) : m_pEnum(pEnum) {
            (*this)++;
        }
        /// <summary>
        /// The type of the out parameter in Next(...) that corresponds to the element type being enumerated.
        /// e.g. PITEMID_CHILD, IDiaSymbol, etc.
        /// </summary>
        using TElem = Details::NextArgType<TEnum>;
        ~enum_iterator() = default;

        auto& operator++(int) { return this->operator++(); }
        auto& operator++() {
            TElem* pChild{ nullptr };
            ULONG celt = 0;
            auto hr = m_pEnum->Next(1, &pChild, &celt);
            if (hr == S_OK && celt == 1) {
                m_current = pChild;
            }
            else if (hr == S_FALSE) {
                m_end = true;
            }
            else {
                THROW_HR(hr);
            }
            return *this;
        }

        TElem* const& operator*() const noexcept { return m_current; }
        TElem* operator->() const noexcept { return m_current; }

        auto operator+(int v) {
            auto other = *this;
            for (int i = 0; i < v; i++) {
                other++;
            }
            return other;
        }

        bool operator==(const enum_iterator& o) const noexcept {
            if (m_end ^ o.m_end) return false;
            if (m_end == o.m_end && m_end == true) return true;
            return o.m_current == this->m_current && o.m_pEnum == m_pEnum && m_end == o.m_end;
        }

        bool operator!=(const enum_iterator& o) const noexcept { return !(*this == o); }

        static constexpr auto end() noexcept {
            return enum_iterator{ end_tag{} };
        }
    protected:
        struct end_tag {};
        enum_iterator(end_tag) : m_end(true) {}
        enum_iterator() {};
        wil::com_ptr<TEnum> m_pEnum{ nullptr };
        TElem* m_current{ nullptr };
        bool m_end{ false };
    };
}

namespace wistd {
    template<typename TEnum>
    auto begin(TEnum* pEnum) {
        return wil::enum_iterator<TEnum>(pEnum);
    }
    template<typename TEnum>
    constexpr auto end(TEnum*) {
        return wil::enum_iterator<TEnum>::template end();
    }
}

#endif